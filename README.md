*This project has been created as part of the 42 curriculum by hyeonwki*

# Webserv

## Description

Webserv is a small HTTP server written in C++98. The goal of the project is to
understand how a web server works by implementing the core pieces directly:
TCP sockets, HTTP request parsing, routing from a configuration file, static
file delivery, uploads, DELETE requests, CGI execution, and non-blocking I/O.

The server is inspired by the structure of an NGINX `server` block, but it does
not use NGINX or any external HTTP library. All request parsing, response
generation, routing, and configuration parsing are implemented in the project.

At runtime, the server:

```text
accepts TCP connections
parses HTTP requests
selects a server and location from the config file
handles static files, upload, DELETE, redirects, directory listing, or CGI
sends an HTTP response without blocking the whole server
```

## Features

- C++98 implementation
- Build target named `webserv`
- Configuration file support
- Default configuration path when no argument is provided
- Non-blocking sockets and CGI pipes
- Single `poll()` driven event loop for listen sockets, clients, and CGI pipes
- GET, POST, DELETE, and HEAD parsing
- Accurate status codes for common error cases
- Default error pages and configured custom error pages
- Static website serving
- Directory index files
- Directory listing when enabled
- HTTP redirects
- Request body size limits
- File upload support
- Safe DELETE handling
- Path traversal protection
- CGI execution by file extension
- Decoded chunked request bodies before handler or CGI processing
- Multiple listen ports serving different content
- Cookie/session demo
- Multiple CGI interpreter demo

## Instructions

Compile the project:

```sh
make
```

Run with an explicit configuration file:

```sh
./webserv config/default.conf
```

Run with the default configuration path:

```sh
./webserv
```

Then open one of these URLs in a browser:

```text
http://127.0.0.1:8080/
```

For the full demonstration configuration, including the second listen port:

```sh
./webserv config/step20.conf
```

Then open:

```text
http://127.0.0.1:8080/
http://127.0.0.1:8081/
```

Stop the server with `Ctrl-C`.

## Makefile

The Makefile provides the required rules:

```sh
make
make all
make clean
make fclean
make re
```

The project is compiled with:

```sh
c++ -Wall -Wextra -Werror -std=c++98
```

No external libraries or Boost are used.

## Configuration

The configuration format is inspired by NGINX. A basic example:

```conf
server {
    listen 127.0.0.1:8080;
    root ./www;
    index index.html;
    client_max_body_size 10M;

    error_page 404 /errors/404.html;
    error_page 413 /errors/413.html;

    location / {
        methods GET;
        autoindex off;
    }

    location /upload {
        methods GET POST;
        upload on;
        upload_store ./www/uploads;
    }

    location /delete {
        methods DELETE;
        root ./www/uploads;
    }

    location /cgi-bin {
        methods GET POST;
        root ./www/cgi-bin;
        cgi .py /usr/bin/python3;
    }
}
```

Supported directives include:

- `listen`
- `root`
- `index`
- `client_max_body_size`
- `error_page`
- `location`
- `methods`
- `return`
- `autoindex`
- `upload`
- `upload_store`
- `cgi`

## Demonstration Routes

With `config/step20.conf`, the project demonstrates the mandatory and bonus
features:

- `/` serves the static website.
- `/old` redirects to `/`.
- `/listing/` shows directory listing.
- `/private-directory/` returns an error when listing is disabled.
- `/has-index/` serves a configured index file.
- `/echo` echoes POST bodies and is useful for chunked-body tests.
- `/upload` accepts file uploads.
- `/delete/<file>` deletes uploaded files.
- `/cgi-bin/hello.py` runs Python CGI.
- `/cgi-bin/hello.sh` runs shell CGI.
- `/session` demonstrates cookies and session storage.
- Port `8081` serves a different site root.

## CGI

CGI programs are selected by file extension in the configuration file. The
server creates pipes, forks only for CGI, connects the request body to the CGI
stdin, reads the CGI stdout, and converts it into an HTTP response.

The CGI environment includes request method, URI, script path, query string,
content headers, server information, remote address, and HTTP request headers.
Chunked request bodies are decoded before being passed to CGI, and EOF marks the
end of the CGI input body.

## Testing

Run the full local test suite:

```sh
./tests/integration.sh
```

Run the same suite without the valgrind smoke check:

```sh
SKIP_VALGRIND=1 ./tests/integration.sh
```

Run the valgrind smoke test only:

```sh
python3 tests/valgrind_smoke.py
```

Useful manual checks:

```sh
curl -i http://127.0.0.1:8080/
curl -i http://127.0.0.1:8081/
curl -i http://127.0.0.1:8080/no-such-file
curl -i http://127.0.0.1:8080/old
curl -i http://127.0.0.1:8080/listing/
curl -i "http://127.0.0.1:8080/cgi-bin/hello.py?name=eval"
curl --path-as-is -i http://127.0.0.1:8080/../../etc/passwd
```

The repository also contains Python tests for:

- configuration parsing
- invalid configuration rejection
- static routing
- uploads and DELETE
- malformed requests
- chunked requests
- slow clients and disconnects
- non-blocking CGI behavior
- concurrent stress requests
- cookie/session bonus behavior
- multiple CGI types

## Project Structure

```text
.
├── Makefile
├── config/
├── include/webserv/
├── src/
│   ├── config/
│   ├── core/
│   ├── handlers/
│   ├── http/
│   └── utils/
├── tests/
└── www/
    ├── cgi-bin/
    ├── errors/
    ├── uploads/
    └── site_b/
```

Key modules:

- `ConfigParser`: parses configuration files.
- `Server`: creates non-blocking listen sockets.
- `EventLoop`: runs the single `poll()` loop.
- `RequestParser`: parses HTTP request lines, headers, and bodies.
- `Router`: selects the effective server/location configuration.
- `StaticFileHandler`: serves files, indexes, and autoindex pages.
- `UploadHandler`: stores raw and multipart uploads.
- `DeleteHandler`: deletes files safely.
- `CgiHandler`: starts CGI processes and builds CGI responses.
- `ResponseBuilder`: creates HTTP responses.
- `PathPolicy`: normalizes URI paths and blocks traversal.

## Resources

- RFC 9110: HTTP Semantics
- RFC 9112: HTTP/1.1
- MDN Web Docs: HTTP overview, methods, status codes, headers
- Linux man pages: `socket`, `bind`, `listen`, `accept`, `poll`, `fcntl`,
  `recv`, `send`, `pipe`, `fork`, `execve`, `waitpid`
- NGINX documentation: server blocks, location matching, root, index, error
  pages, client body size, and CGI-like upstream concepts
- Python CGI documentation and general CGI environment variable references

AI was used as a learning and review assistant for this project. It helped with
summarizing the subject, organizing notes about HTTP/NGINX/configuration files,
building test checklists, improving README wording, and suggesting cases to
verify manually. All implementation decisions, code changes, and final behavior
were reviewed and tested by the project author.
