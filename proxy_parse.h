/*
 * proxy_parse.h -- a HTTP Request Parsing Library.
 *
 * Written by: Matvey Arye, refactored to C++ by Gemini Code Assist.
 */
#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <string_view>
#include <algorithm>
 
#ifndef PROXY_PARSE
#define PROXY_PARSE

/* 
   ParsedRequest objects are created from parsing a buffer containing a HTTP
   request.
 */
class ParsedRequest {
public:
    struct ParsedHeader {
        std::string key;
        std::string value;
    };

    ParsedRequest() = default;
    ~ParsedRequest() = default;

    // Disable copy and assignment
    ParsedRequest(const ParsedRequest&) = delete;
    ParsedRequest& operator=(const ParsedRequest&) = delete;

    // Parse the request buffer
    int parse(const char* buf, int buflen);

    // Unparse the request into a string
    std::string unparse();

    // Unparse only headers into a string
    std::string unparse_headers();

    // Getters
    const std::string& get_method() const { return method; }
    const std::string& get_protocol() const { return protocol; }
    const std::string& get_host() const { return host; }
    const std::string& get_port() const { return port; }
    const std::string& get_path() const { return path; }
    const std::string& get_version() const { return version; }

    // Header manipulation
    void set_header(const std::string& key, const std::string& value);
    ParsedHeader* get_header(std::string_view key);
    bool remove_header(const std::string& key);

private:
    std::string method;
    std::string protocol;
    std::string host;
    std::string port;
    std::string path;
    std::string version;
    std::vector<ParsedHeader> headers;
};

/* Example usage:

   const char *c = 
   "GET http://www.google.com:80/index.html/ HTTP/1.0\r\nContent-Length:"
   " 80\r\nIf-Modified-Since: Sat, 29 Oct 1994 19:43:31 GMT\r\n\r\n";
   
   auto req = std::make_unique<ParsedRequest>();
   if (req->parse(c, strlen(c)) < 0) {
       printf("parse failed\n");
       return -1;
   }

   printf("Method:%s\n", req->get_method().c_str());
   printf("Host:%s\n", req->get_host().c_str());

   // Turn ParsedRequest into a string. 
   std::string request_str = req->unparse();
   printf("%s", request_str.c_str());

   // Turn the headers from the request into a string.
   std::string headers_str = req->unparse_headers();
   printf("%s", headers_str.c_str());

   // Get a specific header (key) from the headers. A key is a header field 
   // such as "If-Modified-Since" which is followed by ":"
   auto* r = req->get_header("If-Modified-Since");
   if (r) {
       printf("Modified value: %s\n", r->value.c_str());
   }
   
   // Remove a specific header by name. In this case remove
   // the "If-Modified-Since" header. 
   if (!req->remove_header("If-Modified-Since")){
      printf("remove header key not work\n");
     return -1;
   }

   // Set a specific header (key) to a value. In this case,
   //we set the "Last-Modified" key to be set to have as 
   //value  a date in February 2014 
   
    req->set_header("Last-Modified", " Wed, 12 Feb 2014 12:43:31 GMT");

   // Check the modified Header key value pair
    r = req->get_header("Last-Modified");
    if (r) {
        printf("Last-Modified value: %s\n", r->value.c_str());
    }
*/

#endif
