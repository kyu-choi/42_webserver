*This project has been created as part of the 42 curriculum by kyu-choi*

# Webserv

`webserv` is a lightweight HTTP server written in **C++98**.

The goal of this project is to understand how a web server works internally by implementing the core parts of HTTP communication directly: socket creation, client connection handling, request parsing, response generation, static file serving, file upload, DELETE requests, CGI execution, configuration parsing, and non-blocking I/O.

This project is inspired by the behavior and configuration style of NGINX, but it is implemented from scratch using only the functions allowed by the 42 subject.

---

## Table of Contents

- [Description](#description)
- [Core Concepts](#core-concepts)
- [Features](#features)
- [Project Architecture](#project-architecture)
- [Instructions](#instructions)
- [Build](#build)
- [Usage](#usage)
- [Configuration File](#configuration-file)
- [HTTP Request Handling Flow](#http-request-handling-flow)
- [Supported Methods](#supported-methods)
- [Status Codes](#status-codes)
- [CGI](#cgi)
- [Testing](#testing)
- [Repository Structure](#repository-structure)
- [Resources](#resources)
- [AI Usage](#ai-usage)

---

## Description

Webserv is an HTTP server that can communicate with a real web browser.

When a browser connects to the server, the server receives an HTTP request, parses it, matches the requested URI with the configuration file, performs the required action, builds an HTTP response, and sends it back to the client.

Basic request/response flow:

```text
Browser
  -> TCP connection
  -> HTTP request
  -> webserv
  -> request parsing
  -> route matching
  -> static file / upload / DELETE / CGI processing
  -> HTTP response
  -> Browser
```

The project focuses on understanding the following topics:

- TCP socket programming
- HTTP request and response structure
- Non-blocking I/O
- Event-driven server design
- `poll`, `select`, `epoll`, or `kqueue`
- Per-client buffer management
- Configuration-based routing
- Static file serving
- File upload
- CGI execution
- Error handling and HTTP status codes

---

## Core Concepts

### HTTP

HTTP is the protocol used by browsers and web servers to exchange data.

Example HTTP request:

```http
GET /index.html HTTP/1.1
Host: localhost:8080
```

Example HTTP response:

```http
HTTP/1.1 200 OK
Content-Type: text/html
Content-Length: 42

<html><body>Hello Webserv</body></html>
```

### Socket

A socket is a communication endpoint used by programs to exchange data over a network.

A basic TCP server follows this flow:

```text
socket()
  -> bind()
  -> listen()
  -> accept()
  -> recv/read()
  -> send/write()
  -> close()
```

In this project, sockets must be handled in a non-blocking way.

### Non-blocking Event Loop

The server must not stop because of one slow client.

Instead of blocking on `read()` or `write()`, the server uses an event monitoring function such as `poll()`.

```text
while (server is running)
{
    wait for events with poll/select/epoll/kqueue

    if a listen socket is readable:
        accept a new client

    if a client socket is readable:
        receive request data

    if a client socket is writable:
        send response data

    if a CGI pipe is readable or writable:
        process CGI data
}
```

---

## Features

Mandatory features expected from this project:

- C++98 implementation
- HTTP server executable named `webserv`
- Configuration file support
- Multiple listening ports
- Non-blocking I/O
- One event loop for client/server I/O
- Browser-compatible HTTP responses
- Accurate HTTP status codes
- Default error pages
- Static website serving
- GET method
- POST method
- DELETE method
- File upload support
- Directory listing when enabled
- Redirection support
- CGI execution based on file extension
- Request body size limit
- Custom error pages
- Stress-test resistant behavior

Optional bonus features:

- Cookies and session management
- Multiple CGI types

---

## Project Architecture

A clean implementation can be separated into the following modules:

```text
webserv
├── ConfigParser
├── Server
├── EventLoop
├── Client
├── HttpRequest
├── RequestParser
├── HttpResponse
├── ResponseBuilder
├── Router
├── StaticFileHandler
├── UploadHandler
├── DeleteHandler
├── CgiHandler
├── ErrorPageHandler
└── Utils
```

### Suggested Responsibilities


| Module              | Responsibility                                                    |
| ------------------- | ----------------------------------------------------------------- |
| `ConfigParser`      | Parse the configuration file and build server/location settings   |
| `Server`            | Create listen sockets and initialize server state                 |
| `EventLoop`         | Monitor all sockets and pipes with `poll` or equivalent           |
| `Client`            | Store client fd, request buffer, response buffer, and state       |
| `RequestParser`     | Parse request line, headers, body, query string, and chunked body |
| `ResponseBuilder`   | Build valid HTTP responses                                        |
| `Router`            | Match URI with the most appropriate location block                |
| `StaticFileHandler` | Serve HTML, CSS, JS, images, and other static files               |
| `UploadHandler`     | Parse upload requests and store files                             |
| `DeleteHandler`     | Delete files when DELETE is allowed                               |
| `CgiHandler`        | Execute CGI scripts and collect their output                      |
| `ErrorPageHandler`  | Generate default or configured error pages                        |


---

## Instructions

Build and start the demonstration configuration:

```bash
make
./webserv config/default.conf
```

The executable also uses `config/default.conf` when no argument is provided.
The demonstration serves the main site on `127.0.0.1:8080` and a second site
on `127.0.0.1:8081`.

Run the automated integration and stress tests with:

```bash
./tests/integration.sh
```

The test suite covers static files, multiple ports, status codes, redirects,
uploads, DELETE, autoindex, chunked requests, Python and PHP CGI, cookie
sessions, and concurrent clients.

---

## Build

Compile the project with:

```bash
make
```

The Makefile must support at least:

```bash
make
make all
make clean
make fclean
make re
```

The project must compile with:

```bash
c++ -Wall -Wextra -Werror -std=c++98
```

External libraries and Boost are not allowed.

---

## Usage

Run the server with a configuration file:

```bash
./webserv config/default.conf
```

If supported by the implementation, run with the default configuration:

```bash
./webserv
```

Then open a browser and visit:

```text
http://localhost:8080
```

You can also test with `curl`:

```bash
curl -v http://localhost:8080/
```

Or with `telnet`:

```bash
telnet localhost 8080
```

Example manual request:

```http
GET / HTTP/1.1
Host: localhost:8080

```

---

## Configuration File

The configuration file controls how the server behaves.

It should be able to define:

- Interface and port pairs
- Server root directory
- Default index file
- Custom error pages
- Maximum client request body size
- Location-specific rules
- Allowed HTTP methods
- Redirections
- Directory listing
- Upload permissions and upload path
- CGI extension and interpreter

Example configuration:

```conf
server {
    listen 127.0.0.1:8080;
    root ./www;
    index index.html;
    client_max_body_size 10M;

    error_page 404 /errors/404.html;
    error_page 500 /errors/500.html;

    location / {
        methods GET;
        autoindex off;
        index index.html;
    }

    location /upload {
        methods GET POST;
        upload on;
        upload_store ./www/uploads;
        autoindex on;
    }

    location /delete {
        root ./www/uploads;
        methods DELETE;
    }

    location /old {
        return 301 /new;
    }

    location /cgi-bin {
        root ./www/cgi-bin;
        methods GET POST;
        cgi .py /usr/bin/python3;
    }
}
```

### Location Matching

When multiple locations match the same URI, the server should select the most specific one.

Example:

```text
location /
location /images
location /images/icons
```

Request:

```text
/images/icons/home.png
```

Selected location:

```text
/images/icons
```

This is called longest prefix matching.

---

## HTTP Request Handling Flow

A typical request is processed as follows:

```text
1. Browser connects to a listen socket.
2. The listen fd becomes readable.
3. The server accepts the new connection.
4. The new client fd is set to non-blocking mode.
5. The client fd is added to the event loop.
6. The browser sends an HTTP request.
7. The client fd becomes readable.
8. The server reads data into the client's request buffer.
9. The server checks whether the request is complete.
10. The request is parsed.
11. The matching server/location configuration is selected.
12. The HTTP method is validated.
13. Static file, upload, DELETE, redirect, or CGI logic is executed.
14. An HTTP response is generated.
15. The client fd becomes writable.
16. The response is sent progressively.
17. The connection is kept alive or closed depending on the request/response.
```

---

## Supported Methods

### GET

Used to request a resource.

Example:

```http
GET /index.html HTTP/1.1
Host: localhost:8080
```

Expected behavior:

```text
- Match the URI with a location
- Build the real file path
- Check if the file exists
- Check access permissions
- Serve the file with the correct Content-Type
- If the path is a directory, serve index or autoindex
```

### POST

Used to send data to the server.

Common use cases:

```text
- HTML form submission
- File upload
- CGI input
```

Expected behavior:

```text
- Read the full request body
- Check Content-Length or Transfer-Encoding
- Enforce client_max_body_size
- Process upload or pass body to CGI
```

### DELETE

Used to delete a resource.

Expected behavior:

```text
- Match the requested path
- Check if DELETE is allowed
- Check if the target exists
- Check permissions
- Delete the file
- Return an accurate status code
```

---

## Status Codes

The server should return accurate HTTP status codes.

Common status codes:


| Code | Meaning               | Typical Case                          |
| ---- | --------------------- | ------------------------------------- |
| 200  | OK                    | Successful GET or POST                |
| 201  | Created               | File uploaded or resource created     |
| 204  | No Content            | Successful DELETE with no body        |
| 301  | Moved Permanently     | Permanent redirect                    |
| 302  | Found                 | Temporary redirect                    |
| 400  | Bad Request           | Invalid HTTP request                  |
| 403  | Forbidden             | Access denied or autoindex disabled   |
| 404  | Not Found             | Resource does not exist               |
| 405  | Method Not Allowed    | Method is not allowed in the location |
| 408  | Request Timeout       | Request took too long                 |
| 413  | Payload Too Large     | Body exceeds configured limit         |
| 414  | URI Too Long          | URI is too long                       |
| 500  | Internal Server Error | Unexpected server-side error          |
| 501  | Not Implemented       | Unsupported method or feature         |
| 502  | Bad Gateway           | Invalid CGI response                  |
| 504  | Gateway Timeout       | CGI timeout                           |


---

## CGI

CGI allows the server to execute an external program and use its output as the HTTP response.

Example:

```text
GET /cgi-bin/hello.py HTTP/1.1
```

Configuration example:

```conf
location /cgi-bin {
    root ./www/cgi-bin;
    methods GET POST;
    cgi .py /usr/bin/python3;
}
```

Basic CGI flow:

```text
1. Detect CGI request by file extension.
2. Build the CGI file path.
3. Prepare CGI environment variables.
4. Create pipes for stdin and stdout.
5. fork().
6. In the child process:
   - connect stdin/stdout with dup2()
   - execute CGI with execve()
7. In the parent process:
   - write request body to CGI stdin
   - read CGI stdout
   - build the final HTTP response
```

Important CGI environment variables may include:

```text
REQUEST_METHOD
SCRIPT_NAME
SCRIPT_FILENAME
QUERY_STRING
CONTENT_LENGTH
CONTENT_TYPE
SERVER_PROTOCOL
SERVER_NAME
SERVER_PORT
REMOTE_ADDR
PATH_INFO
GATEWAY_INTERFACE
```

For chunked requests, the server must unchunk the body before passing it to CGI.

---

## Testing

Recommended testing targets:

### Build Test

```bash
make re
```

### Basic Browser Test

Open:

```text
http://localhost:8080
```

Check:

```text
- index page loads
- CSS loads
- images load
- JavaScript loads if present
```

### GET Test

```bash
curl -v http://localhost:8080/index.html
```

### 404 Test

```bash
curl -v http://localhost:8080/not_found
```

### Method Not Allowed Test

```bash
curl -v -X DELETE http://localhost:8080/
```

### Upload Test

```bash
curl -v -F "file=@test.txt" http://localhost:8080/upload
```

### DELETE Test

```bash
curl -v -X DELETE http://localhost:8080/delete/test.txt
```

### Redirect Test

```bash
curl -v http://localhost:8080/old
```

### CGI GET Test

```bash
curl -v "http://localhost:8080/cgi-bin/hello.py?name=webserv"
```

### CGI POST Test

```bash
curl -v -X POST -d "name=webserv" http://localhost:8080/cgi-bin/hello.py
```

### Stress Test

Use several clients or scripts to confirm that the server remains available and does not crash.

Examples:

```bash
siege http://localhost:8080/
```

or a custom Python script that opens multiple connections.

---

## Repository Structure

A possible repository layout:

```text
.
├── Makefile
├── README.md
├── config
│   ├── default.conf
│   └── test.conf
├── include
│   ├── ConfigParser.hpp
│   ├── Server.hpp
│   ├── EventLoop.hpp
│   ├── Client.hpp
│   ├── HttpRequest.hpp
│   ├── HttpResponse.hpp
│   ├── RequestParser.hpp
│   ├── ResponseBuilder.hpp
│   ├── Router.hpp
│   ├── StaticFileHandler.hpp
│   ├── UploadHandler.hpp
│   ├── DeleteHandler.hpp
│   ├── CgiHandler.hpp
│   └── Utils.hpp
├── src
│   ├── main.cpp
│   ├── config
│   ├── server
│   ├── http
│   ├── handlers
│   └── utils
├── www
│   ├── index.html
│   ├── style.css
│   ├── upload.html
│   ├── errors
│   │   ├── 404.html
│   │   └── 500.html
│   ├── uploads
│   └── cgi-bin
│       └── hello.py
└── tests
    ├── curl_tests.sh
    └── stress_test.py
```

This structure is only a suggestion and may be changed depending on the implementation.

---

## Resources

Useful references for this project:

- RFC 1945 - Hypertext Transfer Protocol HTTP/1.0
- RFC 2616 - Hypertext Transfer Protocol HTTP/1.1
- RFC 3875 - The Common Gateway Interface
- MDN Web Docs - HTTP
- MDN Web Docs - HTTP request methods
- MDN Web Docs - HTTP response status codes
- NGINX documentation
- Linux man pages:
  - `socket`
  - `bind`
  - `listen`
  - `accept`
  - `poll`
  - `select`
  - `fcntl`
  - `read`
  - `write`
  - `send`
  - `recv`
  - `fork`
  - `execve`
  - `pipe`
  - `dup2`
  - `waitpid`
  - `stat`
  - `opendir`
  - `readdir`

---

## AI Usage

AI was used as a learning and documentation assistant during the preparation of this project.

It was used for:

- Clarifying HTTP request/response concepts
- Summarizing the Webserv subject requirements
- Organizing notes about socket programming and event-driven servers
- Explaining NGINX-like configuration concepts
- Drafting and reviewing the C++98 implementation structure
- Generating initial implementation and test scaffolding
- Debugging compilation and CGI integration issues
- Suggesting and implementing testing scenarios

All AI-generated explanations and suggestions must be reviewed, understood, tested, and adapted by the project author before being included in the final implementation.
Every part of the implementation should be understood and explainable during peer evaluation.

---

## Evaluation Notes

During evaluation, be prepared to explain:

- Why the server must be non-blocking
- How `poll` or equivalent event monitoring is used
- How client request buffers are managed
- How partial reads and partial writes are handled
- How request parsing works
- How route matching works
- How `Content-Length` is calculated
- How file upload is processed
- How CGI communicates with the server through pipes
- How client disconnection is handled
- How default and custom error pages are generated

The server should remain operational even when receiving malformed requests, large bodies, disconnected clients, or invalid paths.
