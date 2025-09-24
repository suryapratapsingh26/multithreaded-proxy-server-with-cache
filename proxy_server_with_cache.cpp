// On Windows, winsock2.h must be included before windows.h.
// proxy_parse.h includes windows.h, so we include socket headers first.

#include <winsock2.h> // For socket functions
#include <ws2tcpip.h> // For gethostbyname

#include "proxy_parse.h"
#include <iostream>
#include <string>
#include <vector>
#include <list>
#include <chrono>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <memory>

#define MAX_BYTES 4096    //max allowed size of request/response
#define MAX_CLIENTS 400     //max number of client requests served at a time
#define MAX_SIZE 200*(1<<20)     //size of the cache
#define MAX_ELEMENT_SIZE 10*(1<<20)     //max size of an element in cache


// A C++ class for cache elements. It uses std::string to manage memory automatically.
class CacheElement {
public:
    std::string data;
    std::string url;
    time_t lru_time_track;
    CacheElement* next;
};

using socket_t = SOCKET;
const socket_t INVALID_SOCKET_VAL = INVALID_SOCKET;
void close_socket(socket_t s) { closesocket(s); }

class Semaphore {
public:
    Semaphore(int count = 0) : count_(count) {}

    void post() {
        std::unique_lock<std::mutex> lock(mutex_);
        count_++;
        cv_.notify_one();
    }

    void wait() {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this]() { return count_ > 0; });
        count_--;
    }

private:
    std::mutex mutex_;
    std::condition_variable cv_;
    int count_;
};

std::mutex cout_lock; // Mutex to protect std::cout and std::cerr


CacheElement* find(const std::string& url);
int add_cache_element(const std::string& data, const std::string& url);
void remove_cache_element_nolock(); // Internal version that doesn't lock
void evict_lru_element(); // Public version that locks

int sendErrorMessage(socket_t socket, int status_code)
{
	char str[1024];
	char currentTime[50];
	time_t now = time(0);

	struct tm data = *gmtime(&now);
	strftime(currentTime,sizeof(currentTime),"%a, %d %b %Y %H:%M:%S %Z", &data);

	switch(status_code)
	{
		case 400: snprintf(str, sizeof(str), "HTTP/1.1 400 Bad Request\r\nContent-Length: 95\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>400 Bad Request</TITLE></HEAD>\n<BODY><H1>400 Bad Rqeuest</H1>\n</BODY></HTML>", currentTime);
				  { std::lock_guard<std::mutex> guard(cout_lock); std::cout << "400 Bad Request\n"; }
				  send(socket, str, strlen(str), 0);
				  break;

		case 403: snprintf(str, sizeof(str), "HTTP/1.1 403 Forbidden\r\nContent-Length: 112\r\nContent-Type: text/html\r\nConnection: keep-alive\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>403 Forbidden</TITLE></HEAD>\n<BODY><H1>403 Forbidden</H1><br>Permission Denied\n</BODY></HTML>", currentTime);
				  { std::lock_guard<std::mutex> guard(cout_lock); std::cout << "403 Forbidden\n"; }
				  send(socket, str, strlen(str), 0);
				  break;

		case 404: snprintf(str, sizeof(str), "HTTP/1.1 404 Not Found\r\nContent-Length: 91\r\nContent-Type: text/html\r\nConnection: keep-alive\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>404 Not Found</TITLE></HEAD>\n<BODY><H1>404 Not Found</H1>\n</BODY></HTML>", currentTime);
				  { std::lock_guard<std::mutex> guard(cout_lock); std::cout << "404 Not Found\n"; }
				  send(socket, str, strlen(str), 0);
				  break;

		case 500: snprintf(str, sizeof(str), "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 115\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>500 Internal Server Error</TITLE></HEAD>\n<BODY><H1>500 Internal Server Error</H1>\n</BODY></HTML>", currentTime);
				  //{ std::lock_guard<std::mutex> guard(cout_lock); std::cout << "500 Internal Server Error\n"; }
				  send(socket, str, strlen(str), 0);
				  break;

		case 501: snprintf(str, sizeof(str), "HTTP/1.1 501 Not Implemented\r\nContent-Length: 103\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>404 Not Implemented</TITLE></HEAD>\n<BODY><H1>501 Not Implemented</H1>\n</BODY></HTML>", currentTime);
				  { std::lock_guard<std::mutex> guard(cout_lock); std::cout << "501 Not Implemented\n"; }
				  send(socket, str, strlen(str), 0);
				  break;

		case 505: snprintf(str, sizeof(str), "HTTP/1.1 505 HTTP Version Not Supported\r\nContent-Length: 125\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>505 HTTP Version Not Supported</TITLE></HEAD>\n<BODY><H1>505 HTTP Version Not Supported</H1>\n</BODY></HTML>", currentTime);
				  { std::lock_guard<std::mutex> guard(cout_lock); std::cout << "505 HTTP Version Not Supported\n"; }
				  send(socket, str, strlen(str), 0);
				  break;

		default:  return -1;

	}
	return 1;
}

socket_t connectRemoteServer(const std::string& host_addr, int port_num)
{
	// Creating Socket for remote server ---------------------------

	socket_t remoteSocket = socket(AF_INET, SOCK_STREAM, 0);

	if( remoteSocket == INVALID_SOCKET_VAL)
	{
		{ std::lock_guard<std::mutex> guard(cout_lock); std::cerr << "Error in Creating Socket.\n"; }
		return -1;
	}
	
	// Get host by the name or ip address provided

	struct hostent *host = gethostbyname(host_addr.c_str());	
	if(host == NULL)
	{
		{ std::lock_guard<std::mutex> guard(cout_lock); std::cerr << "No such host exists.\n"; }
		return -1;
	}

	// inserts ip address and port number of host in struct `server_addr`
	struct sockaddr_in server_addr;

	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port_num);

	memcpy(&server_addr.sin_addr.s_addr, host->h_addr, host->h_length);

	// Connect to Remote server ----------------------------------------------------

	if( connect(remoteSocket, (struct sockaddr*)&server_addr, (socklen_t)sizeof(server_addr)) < 0 )
	{
		{ std::lock_guard<std::mutex> guard(cout_lock); std::cerr << "Error in connecting !\n"; }
		return -1;
	}
	// free(host_addr);
	return remoteSocket;
}


int handle_request(socket_t clientSocket, ParsedRequest& request, const std::string& tempReq)
{
	request.set_header("Connection", "close");

	if(request.get_header("Host") == nullptr)
	{
		request.set_header("Host", request.get_host());
	}

    std::string http_request = "GET " + request.get_path() + " " + request.get_version() + "\r\n" + request.unparse_headers();

	int server_port = 80;				// Default Remote Server Port
	if(!request.get_port().empty())
		server_port = std::stoi(request.get_port());

	socket_t remoteSocketID = connectRemoteServer(request.get_host().c_str(), server_port);

	if(remoteSocketID < 0)
		return -1;

	send(remoteSocketID, http_request.c_str(), http_request.length(), 0);

	std::vector<char> buffer(MAX_BYTES);
	std::string response_data;
	int bytes_received;

	// First, receive data from the remote server
	bytes_received = recv(remoteSocketID, buffer.data(), MAX_BYTES - 1, 0);

	while(bytes_received > 0)
	{
		// Send the received data to the client
		int bytes_sent_to_client = send(clientSocket, buffer.data(), bytes_received, 0);
		
		if (bytes_sent_to_client < 0)
		{
			{ std::lock_guard<std::mutex> guard(cout_lock); std::cerr << "Error in sending data to client socket.\n"; }
			break;
        }

		response_data.append(buffer.data(), bytes_received);
		bytes_received = recv(remoteSocketID, buffer.data(), MAX_BYTES - 1, 0);
	} 
	add_cache_element(response_data, tempReq);
	{ std::lock_guard<std::mutex> guard(cout_lock); std::cout << "Request handled and cached." << std::endl; }
	
 	close_socket(remoteSocketID);
	return 0;
}

int checkHTTPversion(const std::string& msg)
{
	int version = -1;

	if(msg.rfind("HTTP/1.1", 0) == 0)
	{
		version = 1;
	}
	else if(msg.rfind("HTTP/1.0", 0) == 0)
	{
		version = 1;										// Handling this similar to version 1.1
	}
	else
		version = -1;

	return version;
}


void thread_fn(socket_t socket, Semaphore* semaphore)
{
	// The semaphore is already waited on in main, we just need to post it when we're done.
	int bytes_send_client;
	auto buffer = std::make_unique<char[]>(MAX_BYTES);
	int total_bytes_received = 0;
	bool request_received = false;

	bytes_send_client = recv(socket, buffer.get(), MAX_BYTES, 0); // Receiving the Request of client by proxy server
	if (bytes_send_client > 0) {
		request_received = true;
		total_bytes_received += bytes_send_client;
	}
	
	while(bytes_send_client > 0)
	{
        //loop until u find "\r\n\r\n" in the buffer
		if(strstr(buffer.get(), "\r\n\r\n") == NULL)
		{	
			bytes_send_client = recv(socket, buffer.get() + total_bytes_received, MAX_BYTES - total_bytes_received, 0);
			if (bytes_send_client > 0) total_bytes_received += bytes_send_client;
		}
		else{
			break;
		}
	}

	if (request_received) {
		std::string tempReq(buffer.get());
		
		//checking for the request in cache 
		CacheElement* temp = find(tempReq);

		if( temp != NULL){
			//request found in cache, so sending the response to client from proxy's cache
			send(socket, temp->data.c_str(), temp->data.length(), 0);
			{ std::lock_guard<std::mutex> guard(cout_lock); std::cout << "Data retrieved from the Cache\n\n"; }
		}
		else // This is a cache miss, handle the request
		{
			//Parsing the request
			ParsedRequest request;
			
			//ParsedRequest_parse returns 0 on success and -1 on failure.On success it stores parsed request in
			// the request
			if (request.parse(buffer.get(), total_bytes_received) < 0)
			{
				{ std::lock_guard<std::mutex> guard(cout_lock); std::cerr << "Parsing failed\n"; }
			}
			else
			{	
				if(request.get_method() == "GET")
				{
					if( !request.get_host().empty() && !request.get_path().empty() && (checkHTTPversion(request.get_version()) == 1) )
					{
						if(handle_request(socket, request, tempReq) == -1)
						{	
							sendErrorMessage(socket, 500);
						}
					}
					else
						sendErrorMessage(socket, 500);			// 500 Internal Error
				}
				else
				{
					{ std::lock_guard<std::mutex> guard(cout_lock); std::cout << "This code doesn't support any method other than GET\n"; }
					sendErrorMessage(socket, 501); // Not Implemented
				}
			}
		}
	} else {
		// This block handles the case where no request was received at all.
		if (bytes_send_client == 0) {
			{ std::lock_guard<std::mutex> guard(cout_lock); std::cout << "Client connected and then disconnected without sending data.\n"; }
		} else { // bytes_send_client < 0
			{ std::lock_guard<std::mutex> guard(cout_lock); std::cerr << "Error in receiving from client on initial recv.\n"; }
		}
	}
	
	shutdown(socket, SD_BOTH);
	close_socket(socket);
	semaphore->post(); // Release the semaphore slot
	{ std::lock_guard<std::mutex> guard(cout_lock); std::cout << "Client connection closed, semaphore released." << std::endl; }
}


int main(int argc, char* argv[]) {

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        { std::lock_guard<std::mutex> guard(cout_lock); std::cerr << "WSAStartup failed.\n"; }
        return 1;
    }

	int port_number = 8080;
	socket_t proxy_socketId;
	socket_t client_socketId;
	int client_len;
	struct sockaddr_in server_addr, client_addr; // Address of client and server to be assigned

    Semaphore semaphore(MAX_CLIENTS);

	if(argc == 2)        //checking whether two arguments are received or not
	{
		port_number = atoi(argv[1]);
	}
	else
	{
		{ std::lock_guard<std::mutex> guard(cout_lock); std::cout << "Usage: " << argv[0] << " <port_number>\n"; }
		exit(1);
	}

	{ std::lock_guard<std::mutex> guard(cout_lock); std::cout << "Setting Proxy Server Port : " << port_number << std::endl; }

    //creating the proxy socket
	proxy_socketId = socket(AF_INET, SOCK_STREAM, 0);

	if( proxy_socketId == INVALID_SOCKET_VAL)
	{
		{ std::lock_guard<std::mutex> guard(cout_lock); std::cerr << "Failed to create socket.\n"; }
		exit(1);
	}

	char reuse = 1;
	if (setsockopt(proxy_socketId, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
        { std::lock_guard<std::mutex> guard(cout_lock); std::cerr << "setsockopt(SO_REUSEADDR) failed\n"; }

	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port_number); // Assigning port to the Proxy
	server_addr.sin_addr.s_addr = INADDR_ANY; // Any available adress assigned

    // Binding the socket
	if( bind(proxy_socketId, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0 )
	{
		{ std::lock_guard<std::mutex> guard(cout_lock); std::cerr << "Port is not free\n"; }
		exit(1);
	}
	{ std::lock_guard<std::mutex> guard(cout_lock); std::cout << "Binding on port: " << port_number << std::endl; }

    // Proxy socket listening to the requests
	int listen_status = listen(proxy_socketId, MAX_CLIENTS);

	if(listen_status < 0 )
	{
		{ std::lock_guard<std::mutex> guard(cout_lock); std::cerr << "Error while Listening !\n"; }
		exit(1);
	}

	std::vector<std::thread> threads;

    // Infinite Loop for accepting connections
	while(1)
	{
		
		memset(&client_addr, 0, sizeof(client_addr));			// Clears struct client_addr
		client_len = sizeof(client_addr); 

        // Accepting the connections
		client_socketId = accept(proxy_socketId, (struct sockaddr*)&client_addr,(socklen_t*)&client_len);	// Accepts connection
		if(client_socketId == INVALID_SOCKET_VAL)
		{
			{ std::lock_guard<std::mutex> guard(cout_lock); std::cerr << "Error in Accepting connection !\n"; }
			exit(1);
		}

		// Getting IP address and port number of client
		struct sockaddr_in* client_pt = (struct sockaddr_in*)&client_addr;
		struct in_addr ip_addr = client_pt->sin_addr;
		char str[INET_ADDRSTRLEN];										// INET_ADDRSTRLEN: Default ip address size
		inet_ntop( AF_INET, &ip_addr, str, INET_ADDRSTRLEN );
		{ std::lock_guard<std::mutex> guard(cout_lock); std::cout << "Client is connected with port number: " << ntohs(client_addr.sin_port) << " and ip address: " << str << std::endl; }
		
		semaphore.wait();
        // Create a new thread for the client and detach it immediately.
        // This allows the main loop to continue accepting new connections without blocking.
        // The detached thread is responsible for its own cleanup.
        std::thread(thread_fn, client_socketId, &semaphore).detach();
	}
	close_socket(proxy_socketId);
    WSACleanup();
 	return 0;
}

std::mutex cache_lock;
CacheElement* head = nullptr;
int cache_size = 0;

CacheElement* find(const std::string& url){

// Checks for url in the cache if found returns pointer to the respective cache element or else returns NULL
    CacheElement* site=NULL;
	std::lock_guard<std::mutex> guard(cache_lock);

    if(head!=NULL){
        site = head;
        while (site!=NULL)
        {
            if(site->url == url){
				{ std::lock_guard<std::mutex> guard(cout_lock); std::cout << "\nurl found\n"; }
				// Updating the time_track
				site->lru_time_track = time(NULL);
				break;
            }
            site=site->next;
        }       
    }
	else {
    { std::lock_guard<std::mutex> guard(cout_lock); std::cout << "\nurl not found\n"; }
	}
    return site;
}

void evict_lru_element() {
	std::lock_guard<std::mutex> guard(cache_lock);
	remove_cache_element_nolock();
}

void remove_cache_element_nolock(){
    // If cache is not empty searches for the node which has the least lru_time_track and deletes it
    CacheElement * p ;  	// Cache_element Pointer (Prev. Pointer)
	CacheElement * q ;		// Cache_element Pointer (Next Pointer)
	CacheElement * temp;	// Cache element to remove

	if( head != NULL) { // Cache != empty
		for (q = head, p = head, temp =head ; q -> next != NULL; 
			q = q -> next) { // Iterate through entire cache and search for oldest time track
			if(( (q -> next) -> lru_time_track) < (temp -> lru_time_track)) {
				temp = q -> next;
				p = q;
			}
		}
		if(temp == head) { 
			head = head -> next; /*Handle the base case*/
		} else {
			p->next = temp->next;	
		}
		cache_size -= (temp->data.length() + temp->url.length() + sizeof(CacheElement)); //updating the cache size
		{ std::lock_guard<std::mutex> guard(cout_lock); std::cout << "Cache element evicted. New size: " << cache_size << std::endl; }
		delete temp; // Use delete for objects allocated with new
	} 
}

int add_cache_element(const std::string& data, const std::string& url){
    // Adds element to the cache
	std::lock_guard<std::mutex> guard(cache_lock);

    int data_size = data.length();
    int element_size = data_size + url.length() + sizeof(CacheElement); // Size of the new element
    if(element_size>MAX_ELEMENT_SIZE){
        // If element size is greater than MAX_ELEMENT_SIZE we don't add the element to the cache
        { std::lock_guard<std::mutex> guard(cout_lock); std::cerr << "Element too large for cache.\n"; }
        return 0;
    }
    else
    {   
		while(cache_size + element_size > MAX_SIZE){
            remove_cache_element_nolock();
        }
        CacheElement* element = new (std::nothrow) CacheElement();
        if (!element) {
            { std::lock_guard<std::mutex> guard(cout_lock); std::cerr << "Failed to allocate memory for cache element.\n"; }
            return 0;
        }
        
        element->data = data;
        element->url = url;
		element->lru_time_track = time(NULL);
        element->next = head;
        head = element;
        cache_size+=element_size;
		{ std::lock_guard<std::mutex> guard(cout_lock); std::cout << "Element added to cache. New size: " << cache_size << std::endl; }
        return 1;
    }
    return 0;
}