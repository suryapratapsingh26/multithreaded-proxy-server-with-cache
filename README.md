<h1>Multi-Threaded C++ Proxy Server with Cache</h1>

This project is a multi-threaded HTTP proxy server written in modern C++. It is designed to handle multiple client connections concurrently, parse HTTP GET requests, and cache responses to speed up subsequent requests for the same resource.

The implementation uses C++ features like classes, `std::thread`, and `std::mutex` to build a robust application for the Windows operating system.

## Index

- Features
- Prerequisites for Windows
- How to Run
- How to Test
- Project Concepts
- Limitations

## Features
- **Multi-Threading**: Uses `std::thread` to handle multiple client connections simultaneously. A semaphore is used to limit the number of active threads.
- **LRU Cache**: Implements a simple Least Recently Used (LRU) cache to store web objects. This reduces latency for repeated requests.
- **HTTP GET Parsing**: Parses incoming HTTP GET requests to extract the host, port, and path.


## How to Run

1.  Open a new PowerShell or Command Prompt terminal and navigate to the project directory.
    ```powershell
    cd C:\ProxyServer
    ```
2.  Compile the project using the provided `Makefile`.
    ```powershell
    make -f Makefile.mk
    ```
    This will create an executable named `proxy.exe`.

3.  Run the proxy server, specifying a port number to listen on (e.g., 8080).
    ```powershell
    .\proxy.exe 8080
    ```

## How to Test

1.  **Disable Browser Cache**: Open your web browser's developer tools (F12) and in the "Network" tab, check the "Disable cache" option. This ensures you are testing *your* proxy's cache, not the browser's.
 
2.  **Configure Browser Proxy**: Configure your web browser or operating system to use a manual HTTP proxy.
    *   **Address/Host**: `localhost`
    *   **Port**: `8080` (or the port you are running the proxy on)
 
3.  **Make a Request**: In your browser's address bar, navigate to any `http://` site (e.g., `http://info.cern.ch`). Note: Your current proxy implementation may not support `https://` sites.
    ```
    http://info.cern.ch
    ```
 
4.  **Check the Console (Cache Miss)**: The first time you access the URL, your proxy's terminal will show output indicating the URL was not found in the cache and was added.
    ```
    url not found
    Request handled and cached.
    Element added to cache. New size: ...
    ```

4.  **Refresh the Page (Cache Hit)**: Refresh the page in your browser. Now, the terminal should show that the data was served directly from your proxy's cache.
    ```
    url found
    Data retrieved from the Cache
    ```

## Project Concepts
- **Concurrency**: A `Semaphore` class (built with `std::mutex` and `std::condition_variable`) limits concurrent client connections to `MAX_CLIENTS`.
- **Cache Management**: A custom singly-linked list acts as the cache. A `std::mutex` (`cache_lock`) protects it from race conditions. When the cache is full, the least recently used element is evicted to make space.
- **Networking**: Uses the Windows Sockets API (Winsock) for network communication.

## Limitations
- The server only supports the `GET` method. Other methods like `POST`, `PUT`, etc., will be rejected.
- Thread management is basic (`std::thread::detach`). A more robust server would use a thread pool for better performance and resource management.
- The current implementation does not support HTTPS requests.
