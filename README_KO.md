*이 프로젝트는 42 커리큘럼의 일부로 kyu-choi가 작성했습니다.*

# Webserv

> 참고: 42 subject 기준 공식 제출용 README는 영어로 작성해야 하며, 이 저장소의 공식 README는 [`README.md`](README.md)(영어)입니다.  
> 이 문서는 프로젝트 이해와 팀 내 공유를 위한 **한국어 버전 README**입니다.

`webserv`는 **C++98**로 구현하는 경량 HTTP 서버입니다.

이 프로젝트의 목표는 웹 서버가 내부적으로 어떻게 동작하는지 직접 구현하면서 이해하는 것입니다. 단순히 HTML 파일 하나를 띄우는 서버가 아니라, 소켓 생성, 클라이언트 연결 처리, HTTP 요청 파싱, HTTP 응답 생성, 정적 파일 제공, 파일 업로드, DELETE 처리, CGI 실행, 설정 파일 파싱, non-blocking I/O까지 직접 구현합니다.

이 프로젝트는 NGINX의 동작 방식과 설정 파일 구조에서 아이디어를 얻을 수 있지만, 42 subject에서 허용한 함수만 사용하여 직접 구현해야 합니다.

---

## 목차

- [프로젝트 설명](#프로젝트-설명)
- [핵심 개념](#핵심-개념)
- [주요 기능](#주요-기능)
- [프로젝트 구조](#프로젝트-구조)
- [빌드](#빌드)
- [실행 방법](#실행-방법)
- [설정 파일](#설정-파일)
- [HTTP 요청 처리 흐름](#http-요청-처리-흐름)
- [지원 메서드](#지원-메서드)
- [상태 코드](#상태-코드)
- [CGI](#cgi)
- [테스트](#테스트)
- [저장소 구조 예시](#저장소-구조-예시)
- [참고 자료](#참고-자료)
- [AI 사용 방식](#ai-사용-방식)
- [평가 대비 포인트](#평가-대비-포인트)

---

## 프로젝트 설명

Webserv는 실제 웹 브라우저와 통신할 수 있는 HTTP 서버입니다.

브라우저가 서버에 접속하면 서버는 HTTP 요청을 수신하고, 요청을 파싱한 뒤, 설정 파일을 기준으로 어떤 동작을 수행할지 결정합니다. 이후 정적 파일 제공, 파일 업로드, DELETE 처리, 리다이렉션, CGI 실행 등을 수행하고 HTTP 응답을 만들어 클라이언트에게 전송합니다.

기본 요청/응답 흐름은 다음과 같습니다.

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

이 프로젝트에서 중점적으로 이해해야 할 주제는 다음과 같습니다.

- TCP 소켓 프로그래밍
- HTTP 요청과 응답 구조
- non-blocking I/O
- 이벤트 기반 서버 구조
- `poll`, `select`, `epoll`, `kqueue` 중 하나를 이용한 이벤트 감시
- 클라이언트별 request/response buffer 관리
- 설정 파일 기반 라우팅
- 정적 파일 제공
- 파일 업로드
- CGI 실행
- 에러 처리와 HTTP 상태 코드

---

## 핵심 개념

### HTTP

HTTP는 브라우저와 웹 서버가 데이터를 주고받기 위한 약속입니다.

예시 HTTP 요청:

```http
GET /index.html HTTP/1.1
Host: localhost:8080
```

예시 HTTP 응답:

```http
HTTP/1.1 200 OK
Content-Type: text/html
Content-Length: 42

<html><body>Hello Webserv</body></html>
```

서버는 요청의 method, URI, header, body를 해석한 뒤 적절한 상태 코드와 body를 포함한 응답을 만들어야 합니다.

---

### 소켓

소켓은 네트워크를 통해 프로그램끼리 데이터를 주고받기 위한 통신 엔드포인트입니다.

기본 TCP 서버 흐름은 다음과 같습니다.

```text
socket()
  -> bind()
  -> listen()
  -> accept()
  -> recv/read()
  -> send/write()
  -> close()
```

Webserv에서는 이 흐름을 단순 blocking 방식으로 구현하면 안 됩니다. 여러 클라이언트를 동시에 처리해야 하므로 소켓은 non-blocking 방식으로 관리해야 합니다.

---

### non-blocking 이벤트 루프

서버는 느린 클라이언트 하나 때문에 전체가 멈추면 안 됩니다.

따라서 `read()`나 `write()`에서 기다리는 방식이 아니라, `poll()` 또는 그에 준하는 이벤트 감시 함수를 사용해 읽기/쓰기 가능한 fd만 처리해야 합니다.

```text
while (server is running)
{
    poll/select/epoll/kqueue로 이벤트 대기

    listen socket이 읽기 가능하면:
        accept로 새 클라이언트 등록

    client socket이 읽기 가능하면:
        요청 데이터 수신

    client socket이 쓰기 가능하면:
        응답 데이터 전송

    CGI pipe가 읽기/쓰기 가능하면:
        CGI 입출력 처리
}
```

중요한 원칙은 다음입니다.

```text
socket, client fd, CGI pipe 같은 fd는
반드시 이벤트 감시 결과를 확인한 뒤 read/write/send/recv 해야 한다.
```

---

## 주요 기능

Mandatory에서 요구되는 핵심 기능은 다음과 같습니다.

- C++98 기반 구현
- 실행 파일 이름 `webserv`
- 설정 파일 지원
- 여러 포트 listen 지원
- non-blocking I/O
- 하나의 이벤트 루프에서 client/server I/O 관리
- 브라우저와 호환되는 HTTP 응답
- 정확한 HTTP status code 반환
- 기본 에러 페이지 제공
- 정적 웹사이트 제공
- GET method 지원
- POST method 지원
- DELETE method 지원
- 파일 업로드 지원
- directory listing 지원
- redirection 지원
- 확장자 기반 CGI 실행
- request body size 제한
- custom error page 지원
- stress test 상황에서도 서버가 죽지 않는 안정성

Bonus 기능은 다음과 같습니다.

- cookie와 session management
- 여러 CGI 타입 지원

---

## 프로젝트 구조

구현을 깔끔하게 나누면 다음과 같은 모듈 구조를 사용할 수 있습니다.

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

### 모듈별 역할


| 모듈                  | 역할                                                         |
| ------------------- | ---------------------------------------------------------- |
| `ConfigParser`      | 설정 파일을 파싱하고 server/location 설정을 생성                         |
| `Server`            | listen socket 생성과 서버 초기화                                   |
| `EventLoop`         | `poll` 또는 equivalent 함수로 모든 socket/pipe 감시                 |
| `Client`            | client fd, request buffer, response buffer, 상태 저장          |
| `RequestParser`     | request line, headers, body, query string, chunked body 파싱 |
| `ResponseBuilder`   | 올바른 HTTP 응답 생성                                             |
| `Router`            | URI와 가장 적절한 location block 매칭                              |
| `StaticFileHandler` | HTML, CSS, JS, 이미지 등 정적 파일 제공                              |
| `UploadHandler`     | 업로드 요청 파싱 및 파일 저장                                          |
| `DeleteHandler`     | DELETE 요청에 따른 파일 삭제 처리                                     |
| `CgiHandler`        | CGI 스크립트 실행 및 출력 수집                                        |
| `ErrorPageHandler`  | 기본 또는 설정된 에러 페이지 생성                                        |


---

## 빌드

프로젝트는 다음 명령어로 컴파일합니다.

```bash
make
```

Makefile은 최소한 다음 규칙을 지원해야 합니다.

```bash
make
make all
make clean
make fclean
make re
```

컴파일 조건은 다음을 만족해야 합니다.

```bash
c++ -Wall -Wextra -Werror -std=c++98
```

외부 라이브러리와 Boost는 사용할 수 없습니다.

---

## 실행 방법

설정 파일을 인자로 전달하여 서버를 실행합니다.

```bash
./webserv config/default.conf
```

구현에서 기본 설정 파일을 지원한다면 다음처럼 실행할 수도 있습니다.

```bash
./webserv
```

서버 실행 후 브라우저에서 접속합니다.

```text
http://localhost:8080
```

`curl`로도 테스트할 수 있습니다.

```bash
curl -v http://localhost:8080/
```

`telnet`으로 직접 HTTP 요청을 보내볼 수도 있습니다.

```bash
telnet localhost 8080
```

수동 요청 예시:

```http
GET / HTTP/1.1
Host: localhost:8080

```

---

## 설정 파일

설정 파일은 서버의 동작을 결정하는 핵심 요소입니다.

설정 파일에서 정의해야 할 주요 항목은 다음과 같습니다.

- interface와 port pair
- server root directory
- default index file
- custom error pages
- maximum client request body size
- location별 규칙
- 허용 HTTP method
- redirection
- directory listing
- upload 허용 여부와 저장 경로
- CGI 확장자와 interpreter

예시 설정 파일:

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

여러 location이 동시에 URI와 매칭될 수 있습니다. 이 경우 가장 구체적인 location을 선택하는 방식이 좋습니다.

예:

```text
location /
location /images
location /images/icons
```

요청:

```text
/images/icons/home.png
```

선택되어야 하는 location:

```text
/images/icons
```

이 방식을 **longest prefix matching**이라고 합니다.

---

## HTTP 요청 처리 흐름

일반적인 요청 하나는 다음 순서로 처리됩니다.

```text
1. 브라우저가 listen socket에 연결한다.
2. listen fd에 readable 이벤트가 발생한다.
3. 서버가 accept()로 새 연결을 수락한다.
4. 새 client fd를 non-blocking으로 설정한다.
5. client fd를 event loop에 등록한다.
6. 브라우저가 HTTP 요청을 보낸다.
7. client fd에 readable 이벤트가 발생한다.
8. 서버가 데이터를 client request buffer에 누적한다.
9. 요청이 완성되었는지 확인한다.
10. 요청을 파싱한다.
11. server/location 설정을 매칭한다.
12. HTTP method 허용 여부를 확인한다.
13. 정적 파일, 업로드, DELETE, redirect, CGI 중 필요한 처리를 수행한다.
14. HTTP 응답을 생성한다.
15. client fd에 writable 이벤트가 발생하면 응답을 전송한다.
16. 응답을 한 번에 다 보내지 못하면 남은 데이터를 buffer에 유지한다.
17. keep-alive 여부에 따라 연결을 유지하거나 닫는다.
```

TCP는 스트림 기반이므로 요청이 항상 한 번의 `recv()`로 완성된다고 가정하면 안 됩니다.

```text
요청이 여러 번에 나뉘어 들어올 수 있다.
요청 두 개가 한 번에 붙어서 들어올 수 있다.
응답도 한 번의 send()로 전부 전송되지 않을 수 있다.
```

따라서 클라이언트별 buffer와 상태 관리가 필수입니다.

---

## 지원 메서드

### GET

GET은 서버의 리소스를 요청하는 method입니다.

예시:

```http
GET /index.html HTTP/1.1
Host: localhost:8080
```

기대 동작:

```text
- URI와 location 매칭
- 실제 파일 경로 생성
- 파일 존재 여부 확인
- 접근 권한 확인
- 올바른 Content-Type으로 파일 응답
- 요청 경로가 디렉토리라면 index 또는 autoindex 처리
```

---

### POST

POST는 클라이언트가 서버에 데이터를 보내는 method입니다.

주요 사용 예:

```text
- HTML form 제출
- 파일 업로드
- CGI 입력 전달
```

기대 동작:

```text
- 전체 request body 수신
- Content-Length 또는 Transfer-Encoding 확인
- client_max_body_size 제한 적용
- 업로드 처리 또는 CGI stdin으로 body 전달
```

---

### DELETE

DELETE는 서버의 리소스를 삭제하는 method입니다.

기대 동작:

```text
- 요청 경로와 location 매칭
- DELETE method 허용 여부 확인
- 삭제 대상 파일 존재 여부 확인
- 접근 권한 확인
- 파일 삭제
- 상황에 맞는 정확한 status code 반환
```

---

## 상태 코드

서버는 상황에 맞는 정확한 HTTP status code를 반환해야 합니다.

자주 사용하는 상태 코드는 다음과 같습니다.


| Code | 의미                    | 일반적인 상황                       |
| ---- | --------------------- | ----------------------------- |
| 200  | OK                    | GET 또는 일반 POST 성공             |
| 201  | Created               | 파일 업로드 또는 리소스 생성 성공           |
| 204  | No Content            | DELETE 성공 후 body 없음           |
| 301  | Moved Permanently     | 영구 리다이렉션                      |
| 302  | Found                 | 임시 리다이렉션                      |
| 400  | Bad Request           | 잘못된 HTTP 요청                   |
| 403  | Forbidden             | 접근 권한 없음 또는 autoindex 비활성화    |
| 404  | Not Found             | 리소스를 찾을 수 없음                  |
| 405  | Method Not Allowed    | 해당 location에서 method가 허용되지 않음 |
| 408  | Request Timeout       | 요청 처리 시간이 너무 오래 걸림            |
| 413  | Payload Too Large     | body가 설정된 제한보다 큼              |
| 414  | URI Too Long          | URI가 너무 김                     |
| 500  | Internal Server Error | 예상하지 못한 서버 내부 오류              |
| 501  | Not Implemented       | 지원하지 않는 method 또는 기능          |
| 502  | Bad Gateway           | CGI 응답이 잘못됨                   |
| 504  | Gateway Timeout       | CGI timeout                   |


---

## CGI

CGI는 서버가 외부 프로그램을 실행하고 그 출력 결과를 HTTP 응답으로 사용하는 방식입니다.

예시 요청:

```http
GET /cgi-bin/hello.py HTTP/1.1
```

설정 예시:

```conf
location /cgi-bin {
    root ./www/cgi-bin;
    methods GET POST;
    cgi .py /usr/bin/python3;
}
```

기본 CGI 처리 흐름:

```text
1. 파일 확장자를 기준으로 CGI 요청인지 판단한다.
2. CGI 파일 경로를 만든다.
3. CGI 환경변수를 구성한다.
4. stdin/stdout 연결용 pipe를 생성한다.
5. fork()를 호출한다.
6. 자식 프로세스에서:
   - dup2()로 stdin/stdout을 pipe에 연결한다.
   - execve()로 CGI 프로그램을 실행한다.
7. 부모 프로세스에서:
   - request body를 CGI stdin으로 전달한다.
   - CGI stdout을 읽는다.
   - 최종 HTTP 응답을 생성한다.
```

대표적인 CGI 환경변수는 다음과 같습니다.

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

chunked request의 경우 CGI에 넘기기 전에 서버가 body를 unchunk해야 합니다.

---

## 테스트

권장 테스트 항목은 다음과 같습니다.

### 빌드 테스트

```bash
make re
```

---

### 브라우저 기본 테스트

브라우저에서 접속합니다.

```text
http://localhost:8080
```

확인 항목:

```text
- index page 로드
- CSS 로드
- 이미지 로드
- JavaScript가 있다면 정상 로드
```

---

### GET 테스트

```bash
curl -v http://localhost:8080/index.html
```

---

### 404 테스트

```bash
curl -v http://localhost:8080/not_found
```

---

### Method Not Allowed 테스트

```bash
curl -v -X DELETE http://localhost:8080/
```

---

### Upload 테스트

```bash
curl -v -F "file=@test.txt" http://localhost:8080/upload
```

---

### DELETE 테스트

```bash
curl -v -X DELETE http://localhost:8080/delete/test.txt
```

---

### Redirect 테스트

```bash
curl -v http://localhost:8080/old
```

---

### CGI GET 테스트

```bash
curl -v "http://localhost:8080/cgi-bin/hello.py?name=webserv"
```

---

### CGI POST 테스트

```bash
curl -v -X POST -d "name=webserv" http://localhost:8080/cgi-bin/hello.py
```

---

### Stress Test

여러 클라이언트나 스크립트를 이용해 서버가 계속 동작하는지 확인합니다.

예:

```bash
siege http://localhost:8080/
```

또는 여러 연결을 여는 Python 테스트 스크립트를 작성할 수 있습니다.

---

## 저장소 구조 예시

가능한 저장소 구조 예시는 다음과 같습니다.

```text
.
├── Makefile
├── README.md
├── README_KR.md
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

이 구조는 예시이며, 실제 구현 방식에 따라 변경할 수 있습니다.

---

## 참고 자료

이 프로젝트를 진행할 때 참고할 수 있는 자료는 다음과 같습니다.

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

## AI 사용 방식

AI는 이 프로젝트를 준비하는 과정에서 학습 및 문서화 보조 도구로 사용되었습니다.

사용된 목적은 다음과 같습니다.

- HTTP request/response 개념 정리
- Webserv subject 요구사항 요약
- 소켓 프로그래밍과 이벤트 기반 서버 구조 정리
- NGINX 스타일 설정 파일 개념 설명
- 문서 구조 초안 작성
- 테스트 시나리오 아이디어 정리

AI가 생성한 설명과 제안은 최종 구현에 포함하기 전에 반드시 직접 검토하고, 이해하고, 테스트하고, 프로젝트 상황에 맞게 수정해야 합니다.

AI는 코드를 이해하는 과정을 대체하기 위한 도구가 아닙니다. 평가 중에는 구현한 모든 부분을 직접 설명할 수 있어야 합니다.

---

## 평가 대비 포인트

평가에서는 다음 내용을 설명할 수 있어야 합니다.

- 왜 서버가 non-blocking이어야 하는가
- `poll` 또는 equivalent 이벤트 감시 함수를 어떻게 사용하는가
- client별 request buffer를 어떻게 관리하는가
- partial read와 partial write를 어떻게 처리하는가
- HTTP request parsing이 어떻게 동작하는가
- route matching을 어떻게 수행하는가
- `Content-Length`를 어떻게 계산하고 검증하는가
- 파일 업로드를 어떻게 처리하는가
- CGI와 서버가 pipe로 어떻게 통신하는가
- 클라이언트 연결 종료를 어떻게 처리하는가
- 기본 에러 페이지와 custom error page를 어떻게 생성하는가

서버는 잘못된 요청, 큰 body, 중간에 끊긴 클라이언트, 잘못된 파일 경로, CGI 오류 같은 상황에서도 종료되지 않고 계속 동작해야 합니다.