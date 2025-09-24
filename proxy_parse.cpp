/*
  proxy_parse.c -- a HTTP Request Parsing Library.
  Refactored to C++ by Gemini Code Assist.
*/

#include "proxy_parse.h"
#include <string>
#include <vector>
#include <sstream>
#include <cstring> // For strstr, strtok_r

#define DEFAULT_NHDRS 8
#define MAX_REQ_LEN 65535
#define MIN_REQ_LEN 4

static const std::string root_abs_path = "/";

#ifdef _WIN32
// strtok_r is not available on Windows, so we use strtok_s
char* strtok_r(char* str, const char* delim, char** saveptr) {
    return strtok_s(str, delim, saveptr);
}
#endif

void ParsedRequest::set_header(const std::string& key, const std::string& value) {
    remove_header(key);
    headers.push_back({key, value});
}

ParsedRequest::ParsedHeader* ParsedRequest::get_header(const std::string& key) {
    for (auto& header : headers) {
        if (strcasecmp(header.key.c_str(), key.c_str()) == 0) {
            return &header;
        }
    }
    return nullptr;
}

bool ParsedRequest::remove_header(const std::string& key) {
    auto it = std::remove_if(headers.begin(), headers.end(),
        [&](const ParsedHeader& h) {
            return strcasecmp(h.key.c_str(), key.c_str()) == 0;
        });
    if (it != headers.end()) {
        headers.erase(it, headers.end());
        return true;
    }
    return false;
}

std::string ParsedRequest::unparse() {
    std::stringstream ss;
    ss << method << " " << protocol << "://" << host;
    if (!port.empty()) {
        ss << ":" << port;
    }
    ss << path << " " << version << "\r\n";
    ss << unparse_headers();
    return ss.str();
}

std::string ParsedRequest::unparse_headers() {
    std::stringstream ss;
    for (const auto& header : headers) {
        ss << header.key << ": " << header.value << "\r\n";
    }
    ss << "\r\n";
    return ss.str();
}

int ParsedRequest::parse(const char* buf, int buflen) {
     char *full_addr;
     char *saveptr;
     char *index;
     char *currentHeader;

     if (buflen < MIN_REQ_LEN || buflen > MAX_REQ_LEN) {
          std::cerr << "invalid buflen " << buflen << std::endl;
          return -1;
      }

     /* Create NUL terminated tmp buffer */
     auto tmp_buf = std::make_unique<char[]>(buflen + 1);
     memcpy(tmp_buf.get(), buf, buflen);
     tmp_buf[buflen] = '\0';
   
     index = strstr(tmp_buf.get(), "\r\n\r\n");
     if (index == NULL) {
          std::cerr << "invalid request line, no end of header" << std::endl;
          return -1;
      }

    index = strstr(tmp_buf.get(), "\r\n");
    std::string request_line(tmp_buf.get(), index - tmp_buf.get());
    auto req_line_buf = std::make_unique<char[]>(request_line.length() + 1);
    strcpy(req_line_buf.get(), request_line.c_str());

     /* Parse request line */
    char* method_ptr = strtok_r(req_line_buf.get(), " ", &saveptr);
    if (method_ptr == nullptr) {
        std::cerr << "invalid request line, no whitespace" << std::endl;
        return -1;
    }
    this->method = method_ptr;

    if (this->method != "GET") {
        std::cerr << "invalid request line, method not 'GET': " << this->method << std::endl;
        return -1;
     }

     full_addr = strtok_r(NULL, " ", &saveptr);

     if (full_addr == NULL) {
        std::cerr << "invalid request line, no full address" << std::endl;
        return -1;
    }

    char* version_ptr = full_addr + strlen(full_addr) + 1;
    if (version_ptr == nullptr || *version_ptr == '\0') {
        std::cerr << "invalid request line, missing version" << std::endl;
        return -1;
    }
    this->version = version_ptr;

    if (this->version.rfind("HTTP/", 0) != 0) {
        std::cerr << "invalid request line, unsupported version " << this->version << std::endl;
        return -1;
    }

    char* protocol_ptr = strtok_r(full_addr, "://", &saveptr);
    if (protocol_ptr == nullptr) {
        std::cerr << "invalid request line, missing protocol" << std::endl;
        return -1;
    }
    this->protocol = protocol_ptr;

    const char* rem = full_addr + strlen(protocol_ptr) + strlen("://");
     size_t abs_uri_len = strlen(rem);

    char* host_ptr = strtok_r(nullptr, "/", &saveptr);
    if (host_ptr == nullptr) {
        std::cerr << "invalid request line, missing host" << std::endl;
        return -1;
    }

    if (strlen(host_ptr) == abs_uri_len) {
        std::cerr << "invalid request line, missing absolute path" << std::endl;
        return -1;
    }

    char* path_ptr = strtok_r(nullptr, " ", &saveptr);
    if (path_ptr == nullptr) {
        this->path = "/";
    } else {
        this->path = std::string("/") + path_ptr;
    }

    char* port_ptr = nullptr;
    char* host_saveptr = nullptr;
    this->host = strtok_r(host_ptr, ":", &host_saveptr);
    port_ptr = strtok_r(nullptr, "/", &host_saveptr);

    if (port_ptr != nullptr) {
        this->port = port_ptr;
        try {
            int port_num = std::stoi(this->port);
            if (port_num <= 0 || port_num > 65535) {
                 std::cerr << "invalid port number: " << this->port << std::endl;
                 return -1;
            }
        } catch (const std::invalid_argument& ia) {
            std::cerr << "invalid request line, bad port: " << this->port << std::endl;
            return -1;
        }
    }

     /* Parse headers */
    currentHeader = strstr(tmp_buf.get(), "\r\n") + 2;
    while (currentHeader[0] != '\0' && !(currentHeader[0] == '\r' && currentHeader[1] == '\n')) {
        char* nextHeader = strstr(currentHeader, "\r\n");
        if (nextHeader == nullptr) break;

        char* colon = strchr(currentHeader, ':');
        if (colon == nullptr || colon > nextHeader) {
            currentHeader = nextHeader + 2;
            continue;
        }

        std::string key(currentHeader, colon - currentHeader);
        char* value_start = colon + 1;
        while (*value_start == ' ') value_start++;
        std::string value(value_start, nextHeader - value_start);

        set_header(key, value);

        currentHeader = nextHeader + 2;
    }

    return 0;
}