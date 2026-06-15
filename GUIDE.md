# Webserv 과제 GUIDE

> 42 **Webserv** 과제는 C++98로 직접 HTTP 서버를 구현하는 프로젝트이다.  
> 목표는 단순히 웹페이지를 띄우는 것이 아니라, 브라우저와 통신 가능한 **non-blocking HTTP 서버**를 직접 설계하고 구현하는 것이다.

---

## 0. 한 줄 요약

Webserv는 다음을 직접 구현하는 과제이다.

```text
브라우저 요청 수신
→ HTTP 요청 파싱
→ 설정 파일 기반으로 route 결정
→ 정적 파일 / 업로드 / DELETE / CGI 처리
→ HTTP 응답 생성
→ non-blocking 방식으로 클라이언트에 전송
```

즉, 이 과제의 핵심은 **HTTP + 소켓 프로그래밍 + 이벤트 기반 서버 + 파일 시스템 + CGI + 설정 파일 파싱**을 모두 묶어서 구현하는 것이다.

---

# 1. 과제의 목적

## 1.1 HTTP가 실제로 어떻게 동작하는지 이해하기

브라우저에서 주소창에 URL을 입력하면 내부적으로 HTTP 요청이 서버로 전송된다.

예를 들어 사용자가 다음 주소에 접속한다고 하자.

```text
http://localhost:8080/index.html
```

브라우저는 서버에 대략 다음과 같은 요청을 보낸다.

```http
GET /index.html HTTP/1.1
Host: localhost:8080
User-Agent: ...
Accept: ...
```

서버는 이 요청을 해석한 뒤 파일을 찾아 응답한다.

```http
HTTP/1.1 200 OK
Content-Type: text/html
Content-Length: 1234

<html>...</html>
```

Webserv 과제는 이 과정을 직접 구현하면서 다음을 이해하도록 만든다.

- HTTP request line 구조
- HTTP header 구조
- HTTP body 처리
- HTTP response status code
- Content-Length의 중요성
- 브라우저와 서버의 통신 방식
- 정적 파일 제공 방식
- 업로드와 CGI 처리 방식

---

## 1.2 NGINX 같은 웹 서버의 축소판 만들기

이 과제에서는 NGINX의 `server` 설정 블록을 참고할 수 있다고 되어 있다.

즉, 단순한 서버 하나를 만드는 것이 아니라 **설정 파일로 동작이 바뀌는 HTTP 서버**를 만들어야 한다.

설정 파일을 통해 다음을 제어할 수 있어야 한다.

```text
어떤 IP와 포트에서 listen 할지
어떤 URL route가 어떤 디렉토리를 root로 사용할지
어떤 HTTP method를 허용할지
업로드를 허용할지
업로드 파일을 어디에 저장할지
에러 페이지를 어떻게 설정할지
디렉토리 listing을 켤지 끌지
CGI를 어떤 확장자에서 실행할지
리다이렉션을 어떻게 처리할지
```

따라서 이 과제의 결과물은 다음과 같은 성격을 가진다.

```text
설정 가능한 HTTP 서버 엔진
```

---

## 1.3 이벤트 기반 non-blocking 서버 구조 익히기

Webserv에서 가장 중요한 요구사항은 **non-blocking I/O**이다.

서버는 여러 클라이언트를 동시에 처리해야 한다.

어떤 클라이언트가 요청을 천천히 보내거나, 응답을 천천히 받는다고 해서 서버 전체가 멈추면 안 된다.

따라서 다음과 같은 구조가 필요하다.

```text
while (server_running)
{
    poll 또는 select 또는 epoll 또는 kqueue로 이벤트 감시

    listen socket에 이벤트가 있으면 accept
    client socket이 읽기 가능하면 recv/read
    client socket이 쓰기 가능하면 send/write
    CGI pipe가 읽기/쓰기 가능하면 처리
    연결이 끊어진 fd는 정리
}
```

핵심은 다음이다.

> `read`, `write`, `recv`, `send`는 마음대로 호출하면 안 된다.  
> 반드시 poll 계열 함수가 해당 fd의 준비 상태를 알려준 뒤 호출해야 한다.

---

# 2. 제출물과 기본 규칙

## 2.1 실행 파일 이름

실행 파일 이름은 반드시 다음이어야 한다.

```text
webserv
```

실행 형태는 다음과 같다.

```bash
./webserv [configuration file]
```

설정 파일이 인자로 주어지지 않으면 기본 경로의 설정 파일을 사용할 수 있어야 한다.

---

## 2.2 제출 파일

제출해야 하는 파일은 다음과 같다.

```text
Makefile
*.h
*.hpp
*.cpp
*.tpp
*.ipp
configuration files
README.md
테스트용 정적 파일
테스트용 에러 페이지
테스트용 CGI 파일
업로드 테스트용 페이지
```

---

## 2.3 Makefile 규칙

Makefile에는 최소한 다음 규칙이 있어야 한다.

```makefile
NAME
all
clean
fclean
re
```

예시 구조는 다음과 같다.

```makefile
NAME = webserv

all: $(NAME)

$(NAME):
	c++ -Wall -Wextra -Werror -std=c++98 ...

clean:
	rm -f $(OBJS)

fclean: clean
	rm -f $(NAME)

re: fclean all
```

주의할 점은 **불필요한 relink가 발생하면 안 된다**는 것이다.

---

## 2.4 컴파일 조건

컴파일은 다음 플래그를 만족해야 한다.

```bash
c++ -Wall -Wextra -Werror
```

또한 C++98 표준으로 컴파일되어야 한다.

```bash
-std=c++98
```

즉, 다음 기능들은 사용하면 안 된다.

```text
C++11 auto
nullptr
lambda
range-based for
std::thread
std::unordered_map
std::filesystem
smart pointer 계열 C++11 기능
```

사용 가능한 대표 컨테이너는 다음과 같다.

```cpp
std::string
std::vector
std::map
std::set
std::list
std::stringstream
```

---

## 2.5 외부 라이브러리 금지

외부 라이브러리와 Boost는 금지이다.

즉, 다음은 사용할 수 없다.

```text
Boost
asio
libevent
libuv
외부 HTTP parser 라이브러리
외부 JSON/YAML parser 라이브러리
```

HTTP 파서, config 파서, route 처리, response 생성 등을 직접 구현해야 한다.

---

# 3. 허용 함수와 의미

과제에서 허용된 함수들은 대부분 네트워크, 파일, 프로세스, CGI 처리를 위한 함수들이다.

## 3.1 소켓 관련 함수

```text
socket
setsockopt
bind
listen
accept
connect
getsockname
getaddrinfo
freeaddrinfo
getprotobyname
```

주요 흐름은 다음과 같다.

```text
socket()
→ setsockopt()
→ bind()
→ listen()
→ poll/select에 listen fd 등록
→ accept()
→ client fd non-blocking 설정
→ poll/select에 client fd 등록
```

---

## 3.2 네트워크 바이트 변환 함수

```text
htons
htonl
ntohs
ntohl
```

포트 번호나 IP 주소 관련 값을 네트워크 바이트 오더로 변환할 때 사용한다.

예:

```cpp
addr.sin_port = htons(8080);
```

---

## 3.3 이벤트 감시 함수

다음 중 하나를 선택해 사용할 수 있다.

```text
poll
select
epoll
kqueue
```

과제 문서에는 `poll()`이 자주 언급되지만, equivalent 함수도 허용된다.

Ubuntu/Linux에서는 보통 다음 중 하나를 선택한다.

```text
poll
select
epoll
```

학습과 구현 난이도를 고려하면 처음에는 `poll`이 가장 무난하다.

---

## 3.4 fd 제어 함수

```text
fcntl
close
read
write
send
recv
```

`fcntl`은 fd를 non-blocking으로 만들 때 사용한다.

예:

```cpp
int flags = fcntl(fd, F_GETFL, 0);
fcntl(fd, F_SETFL, flags | O_NONBLOCK);
```

다만 macOS 관련 제한이 있으므로 평가 환경을 고려해야 한다.

---

## 3.5 파일 시스템 관련 함수

```text
access
stat
open
opendir
readdir
closedir
chdir
```

정적 파일 제공, 디렉토리 listing, CGI 실행 위치 변경 등에 사용된다.

---

## 3.6 CGI 관련 함수

```text
fork
execve
pipe
dup
dup2
waitpid
kill
signal
socketpair
```

CGI는 별도 프로세스로 실행해야 하므로 다음 흐름이 필요하다.

```text
pipe 생성
fork
자식 프로세스에서 dup2로 stdin/stdout 연결
execve로 CGI 실행
부모 프로세스에서 CGI stdin에 body 전달
부모 프로세스에서 CGI stdout 읽기
응답 생성
```

중요한 제한이 있다.

> `fork`는 CGI 이외의 용도로 사용하면 안 된다.

즉, 외부 웹서버를 실행하거나, 요청마다 서버 처리를 fork로 넘기는 구조는 안 된다.

---

# 4. Mandatory 요구사항 전체 정리

Mandatory에서 구현해야 하는 핵심은 다음이다.

```text
1. C++98 HTTP 서버
2. 설정 파일 사용
3. non-blocking I/O
4. 하나의 poll 또는 equivalent 이벤트 루프
5. read/write 전 readiness 확인
6. 브라우저 호환성
7. 정확한 HTTP status code
8. 기본 에러 페이지
9. 정적 웹사이트 제공
10. 파일 업로드
11. GET, POST, DELETE method
12. 여러 포트 listen
13. CGI 실행
14. route별 설정
15. 스트레스 테스트에서도 죽지 않는 안정성
```

---

# 5. 가장 중요한 요구사항: non-blocking I/O

## 5.1 왜 non-blocking이 필요한가?

서버는 여러 클라이언트를 동시에 처리해야 한다.

만약 blocking 방식으로 구현하면 다음 문제가 생긴다.

```text
client A가 요청 body를 천천히 보냄
→ 서버가 client A의 recv에서 대기
→ client B, C, D 요청을 처리하지 못함
→ 서버 전체가 멈춘 것처럼 보임
```

따라서 서버는 한 클라이언트 때문에 멈추면 안 된다.

---

## 5.2 반드시 poll 계열 함수로 readiness 확인

잘못된 방식:

```cpp
recv(client_fd, buffer, sizeof(buffer), 0);
send(client_fd, response.c_str(), response.size(), 0);
```

문제점:

```text
client_fd가 읽기 가능한지 모른다.
client_fd가 쓰기 가능한지 모른다.
호출 순간 blocking될 수 있다.
```

올바른 방식:

```text
poll에서 POLLIN 이벤트 확인
→ recv 호출

poll에서 POLLOUT 이벤트 확인
→ send 호출
```

---

## 5.3 하나의 이벤트 루프에서 모든 I/O 관리

모든 socket, pipe 계열 fd는 하나의 이벤트 루프에서 관리해야 한다.

예:

```text
poll_fds:
  listen_fd_8080    POLLIN
  listen_fd_8081    POLLIN
  client_fd_1       POLLIN | POLLOUT
  client_fd_2       POLLIN
  cgi_stdout_fd     POLLIN
  cgi_stdin_fd      POLLOUT
```

listen socket도 poll 대상에 포함되어야 한다.

---

## 5.4 read/write 후 errno 기반 처리 금지

과제에서는 다음과 같은 방식을 금지한다.

```cpp
ssize_t n = recv(fd, buf, size, 0);
if (n < 0 && errno == EAGAIN)
{
    // 나중에 다시 시도
}
```

의도는 다음이다.

```text
errno로 준비 상태를 추측하지 말고,
poll/select가 알려준 readiness에 따라 read/write를 호출하라.
```

---

## 5.5 일반 디스크 파일은 예외

일반 파일은 poll로 readiness 확인하지 않아도 된다.

예:

```text
HTML 파일 읽기
이미지 파일 읽기
업로드 파일 저장
에러 페이지 파일 읽기
```

하지만 다음은 반드시 non-blocking 이벤트 기반이어야 한다.

```text
socket
client fd
listen fd
pipe
FIFO
CGI stdin/stdout pipe
```

---

# 6. HTTP 요청 파싱

HTTP 요청은 크게 세 부분으로 나뉜다.

```text
Request Line
Headers
Body
```

예:

```http
POST /upload?name=test HTTP/1.1
Host: localhost:8080
Content-Length: 123
Content-Type: multipart/form-data; boundary=----abc

<body data>
```

---

## 6.1 Request Line

첫 줄은 다음 구조이다.

```http
METHOD URI HTTP_VERSION
```

예:

```http
GET /index.html HTTP/1.1
POST /upload HTTP/1.1
DELETE /uploads/a.txt HTTP/1.1
```

파싱해야 하는 값:

```text
method: GET / POST / DELETE / 기타
uri: /index.html, /upload, /cgi/test.py?x=1
path: query string 제거 후 경로
query_string: ? 뒤의 값
http_version: HTTP/1.0 또는 HTTP/1.1
```

---

## 6.2 Header

Header는 key-value 형태이다.

```http
Host: localhost:8080
Content-Length: 123
Connection: keep-alive
Content-Type: text/html
Transfer-Encoding: chunked
```

구현 시 header key는 대소문자 구분을 조심해야 한다.

HTTP header name은 일반적으로 case-insensitive로 다루는 것이 안전하다.

---

## 6.3 Header의 끝

HTTP header의 끝은 다음 문자열로 판단한다.

```text
\r\n\r\n
```

즉, read buffer에서 이 문자열을 찾으면 header 파싱을 시작할 수 있다.

---

## 6.4 Body 처리

Body가 있는 요청은 보통 POST이다.

Body 길이는 보통 `Content-Length`로 판단한다.

```http
Content-Length: 123
```

이 경우 header 이후 정확히 123바이트를 body로 읽어야 한다.

주의할 점:

```text
한 번의 recv로 요청 전체가 오지 않을 수 있다.
header만 먼저 오고 body가 나중에 올 수 있다.
body가 여러 조각으로 나뉘어 올 수 있다.
요청 여러 개가 한 번에 붙어서 올 수 있다.
```

따라서 client별 read buffer가 필요하다.

---

## 6.5 Transfer-Encoding: chunked

chunked 요청은 body가 다음 형식으로 온다.

```http
4\r\n
Wiki\r\n
5\r\n
pedia\r\n
0\r\n
\r\n
```

실제 body는 다음이다.

```text
Wikipedia
```

CGI에 넘길 때는 chunked 형식을 제거한 실제 body만 전달해야 한다.

---

# 7. HTTP 응답 생성

HTTP 응답은 다음 구조이다.

```text
Status Line
Headers
빈 줄
Body
```

예:

```http
HTTP/1.1 200 OK
Content-Type: text/html
Content-Length: 42
Connection: close

<html><body>Hello</body></html>
```

---

## 7.1 Status Line

형식은 다음과 같다.

```http
HTTP/1.1 STATUS_CODE REASON_PHRASE
```

예:

```http
HTTP/1.1 200 OK
HTTP/1.1 404 Not Found
HTTP/1.1 500 Internal Server Error
```

---

## 7.2 필수에 가까운 Header

응답에는 최소한 다음을 넣는 것이 좋다.

```http
Content-Length: <body size>
Content-Type: <mime type>
Connection: close 또는 keep-alive
```

정적 파일이면 Content-Type을 파일 확장자에 따라 정한다.

```text
.html → text/html
.css  → text/css
.js   → application/javascript
.png  → image/png
.jpg  → image/jpeg
.gif  → image/gif
.ico  → image/x-icon
.txt  → text/plain
.json → application/json
.pdf  → application/pdf
```

---

## 7.3 Content-Length는 매우 중요함

Content-Length가 실제 body 길이와 다르면 문제가 생긴다.

가능한 문제:

```text
브라우저가 계속 기다림
응답 일부만 표시됨
다음 요청과 응답이 꼬임
keep-alive가 깨짐
```

따라서 body를 만든 뒤 정확한 바이트 길이를 계산해야 한다.

---

# 8. HTTP Status Code 정리

Webserv에서 자주 필요한 status code는 다음과 같다.

| Code | 의미 | 사용 상황 |
|---:|---|---|
| 200 | OK | GET 성공, 일반 POST 성공 |
| 201 | Created | 업로드 또는 리소스 생성 성공 |
| 204 | No Content | DELETE 성공 후 body 없음 |
| 301 | Moved Permanently | 영구 리다이렉션 |
| 302 | Found | 임시 리다이렉션 |
| 400 | Bad Request | 요청 문법 오류 |
| 403 | Forbidden | 권한 없음, autoindex off인 디렉토리 접근 |
| 404 | Not Found | 파일 또는 route 없음 |
| 405 | Method Not Allowed | route에서 허용하지 않은 method |
| 408 | Request Timeout | 요청이 너무 오래 걸림 |
| 413 | Payload Too Large | body 크기 제한 초과 |
| 414 | URI Too Long | URI가 너무 김 |
| 500 | Internal Server Error | 서버 내부 오류 |
| 501 | Not Implemented | 지원하지 않는 method |
| 502 | Bad Gateway | CGI가 잘못된 응답 또는 실패 |
| 504 | Gateway Timeout | CGI timeout |
|

---

# 9. 설정 파일 요구사항

## 9.1 설정 파일의 역할

설정 파일은 서버의 동작을 결정한다.

예를 들어 다음을 설정한다.

```text
listen할 포트
root 디렉토리
index 파일
허용 method
에러 페이지
body size 제한
업로드 경로
CGI 확장자
redirection
autoindex
```

---

## 9.2 설정 파일에서 지원해야 하는 항목

과제 요구사항 기준으로 설정 파일에서는 다음을 지원해야 한다.

```text
1. interface:port pair 설정
2. default error page 설정
3. client request body 최대 크기 설정
4. route/location별 accepted methods 설정
5. route/location별 HTTP redirection 설정
6. route/location별 root 설정
7. directory listing on/off 설정
8. directory 요청 시 default file 설정
9. upload 허용 여부와 저장 위치 설정
10. CGI 실행 설정
```

---

## 9.3 예시 설정 파일

아래는 이해를 위한 예시이다. 실제 문법은 팀에서 직접 정해도 된다.

```nginx
server {
    listen 127.0.0.1:8080;
    client_max_body_size 10M;

    error_page 404 /errors/404.html;
    error_page 500 /errors/500.html;

    location / {
        root ./www;
        index index.html;
        methods GET;
        autoindex off;
    }

    location /upload {
        root ./www;
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

---

# 10. Route 처리

## 10.1 route란?

Route는 URL 경로별 동작 규칙이다.

예:

```text
/           → 메인 정적 페이지
/upload     → 업로드 페이지 및 업로드 처리
/cgi-bin    → CGI 실행
/images     → 이미지 파일 제공
```

---

## 10.2 route matching

요청 URI가 들어오면 가장 적절한 location을 찾아야 한다.

보통은 **가장 긴 prefix match** 방식이 적절하다.

예:

```text
location /
location /upload
location /upload/images
```

요청:

```text
/upload/images/a.png
```

선택되어야 하는 location:

```text
/upload/images
```

왜냐하면 가장 구체적인 route이기 때문이다.

---

## 10.3 URL을 실제 파일 경로로 변환

예:

```text
location /kapouet {
    root /tmp/www;
}
```

요청:

```text
/kapouet/pouic/toto/pouet
```

실제 파일 경로:

```text
/tmp/www/pouic/toto/pouet
```

즉, location prefix인 `/kapouet`을 제거하고 나머지 경로를 root 뒤에 붙인다.

---

# 11. GET 구현

GET은 서버의 리소스를 요청하는 method이다.

## 11.1 기본 흐름

```text
GET 요청 수신
→ route 찾기
→ method 허용 여부 확인
→ URI를 실제 파일 경로로 변환
→ 파일/디렉토리 존재 확인
→ 파일이면 읽어서 응답
→ 디렉토리면 index 또는 autoindex 처리
→ 실패 시 에러 응답
```

---

## 11.2 파일 요청

요청:

```http
GET /index.html HTTP/1.1
```

처리:

```text
root + /index.html
→ 파일 존재 확인
→ 권한 확인
→ 파일 읽기
→ Content-Type 설정
→ 200 OK 응답
```

---

## 11.3 디렉토리 요청

요청:

```http
GET / HTTP/1.1
```

처리 우선순위:

```text
1. index 파일이 설정되어 있고 존재하면 index 파일 반환
2. index 파일이 없고 autoindex on이면 directory listing 반환
3. index 파일이 없고 autoindex off이면 403 Forbidden
```

---

## 11.4 autoindex

`autoindex on`이면 디렉토리 목록 HTML을 직접 만들어야 한다.

예:

```html
<html>
<body>
<h1>Index of /uploads</h1>
<ul>
  <li><a href="file1.txt">file1.txt</a></li>
  <li><a href="image.png">image.png</a></li>
</ul>
</body>
</html>
```

사용 함수:

```text
opendir
readdir
closedir
```

---

# 12. POST 구현

POST는 클라이언트가 서버에 데이터를 보내는 method이다.

사용 예:

```text
파일 업로드
HTML form 제출
CGI에 body 전달
```

---

## 12.1 POST 기본 흐름

```text
POST 요청 수신
→ header 파싱
→ Content-Length 또는 chunked 확인
→ body 전체 수신
→ body size 제한 확인
→ route 찾기
→ upload 또는 CGI 또는 일반 POST 처리
→ 응답 생성
```

---

## 12.2 client_max_body_size 확인

설정된 최대 body 크기보다 요청 body가 크면 거절해야 한다.

응답:

```http
HTTP/1.1 413 Payload Too Large
```

---

## 12.3 multipart/form-data

파일 업로드 form은 보통 다음 형식이다.

```http
Content-Type: multipart/form-data; boundary=----WebKitFormBoundaryABC
```

body 내부는 boundary로 구분된다.

대략 구조:

```text
------WebKitFormBoundaryABC
Content-Disposition: form-data; name="file"; filename="test.txt"
Content-Type: text/plain

파일 내용
------WebKitFormBoundaryABC--
```

구현해야 할 것:

```text
boundary 추출
각 part 분리
filename 추출
파일 내용 추출
upload_store에 저장
```

---

# 13. 파일 업로드 구현

## 13.1 요구사항

클라이언트는 서버로 파일을 업로드할 수 있어야 한다.

설정 파일에서 route별로 다음을 지정해야 한다.

```text
업로드 허용 여부
업로드 저장 위치
```

예:

```nginx
location /upload {
    methods GET POST;
    upload on;
    upload_store ./www/uploads;
}
```

---

## 13.2 업로드 처리 흐름

```text
POST /upload 요청
→ route가 upload on인지 확인
→ body size 제한 확인
→ multipart/form-data 파싱
→ filename 확인
→ 저장 경로 생성
→ 파일 open/write
→ 성공 시 201 Created 또는 200 OK
→ 실패 시 500 또는 403
```

---

## 13.3 고려해야 할 예외

```text
업로드 route가 아닌데 POST 업로드 요청
Content-Length 없음
body가 너무 큼
multipart boundary 없음
filename 없음
저장 디렉토리 없음
저장 권한 없음
동일 파일명 존재
파일명이 ../ 를 포함함
```

특히 파일명에 `../`가 들어가면 root 밖으로 저장될 수 있으므로 반드시 막아야 한다.

---

# 14. DELETE 구현

DELETE는 서버의 리소스를 삭제하는 method이다.

## 14.1 기본 흐름

```text
DELETE 요청 수신
→ route 찾기
→ method 허용 여부 확인
→ URI를 실제 파일 경로로 변환
→ 파일 존재 확인
→ 권한 확인
→ 삭제
→ 결과에 맞는 status code 응답
```

---

## 14.2 응답 예시

삭제 성공:

```http
HTTP/1.1 204 No Content
```

파일 없음:

```http
HTTP/1.1 404 Not Found
```

권한 없음:

```http
HTTP/1.1 403 Forbidden
```

DELETE가 허용되지 않은 route:

```http
HTTP/1.1 405 Method Not Allowed
```

---

# 15. Redirection 구현

설정 파일에서 특정 route를 다른 URL로 이동시킬 수 있어야 한다.

예:

```nginx
location /old {
    return 301 /new;
}
```

요청:

```http
GET /old HTTP/1.1
```

응답:

```http
HTTP/1.1 301 Moved Permanently
Location: /new
Content-Length: 0
```

필수 header:

```http
Location: /new
```

---

# 16. Error Page 구현

## 16.1 기본 요구사항

설정 파일에 에러 페이지가 없더라도 서버는 기본 에러 페이지를 제공해야 한다.

예:

```html
<html>
<body>
<h1>404 Not Found</h1>
</body>
</html>
```

---

## 16.2 설정 파일의 에러 페이지

예:

```nginx
error_page 404 /errors/404.html;
error_page 500 /errors/500.html;
```

404가 발생하면 `/errors/404.html`을 응답 body로 사용한다.

주의할 점:

```text
에러 페이지 파일 자체가 없을 수 있다.
에러 페이지 파일 접근 권한이 없을 수 있다.
에러 페이지 처리 중 또 에러가 날 수 있다.
```

이런 경우에도 서버가 죽으면 안 된다.

---

# 17. CGI 구현

CGI는 Webserv에서 가장 복잡한 파트 중 하나이다.

CGI는 서버가 외부 프로그램을 실행하고, 그 출력 결과를 HTTP 응답으로 사용하는 방식이다.

예:

```text
브라우저 요청: GET /cgi-bin/hello.py
서버: python3 hello.py 실행
CGI 출력: Content-Type: text/html + body
서버: CGI 출력을 브라우저에 응답
```

---

## 17.1 CGI 기본 흐름

```text
CGI 요청 감지
→ CGI 실행 파일 경로 결정
→ 환경변수 구성
→ pipe 생성
→ fork
→ 자식 프로세스에서 stdin/stdout 연결
→ execve로 CGI 실행
→ 부모 프로세스에서 body를 CGI stdin에 전달
→ 부모 프로세스에서 CGI stdout 읽기
→ CGI 출력 파싱
→ HTTP 응답 생성
```

---

## 17.2 CGI 실행 판단

설정 예:

```nginx
location /cgi-bin {
    root ./www/cgi-bin;
    cgi .py /usr/bin/python3;
}
```

요청:

```http
GET /cgi-bin/test.py HTTP/1.1
```

파일 확장자가 `.py`이면 `/usr/bin/python3`로 실행한다.

---

## 17.3 CGI 환경변수

CGI에 전달해야 할 대표 환경변수는 다음과 같다.

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
REDIRECT_STATUS
```

예:

```text
REQUEST_METHOD=POST
QUERY_STRING=name=kyu
CONTENT_LENGTH=123
CONTENT_TYPE=application/x-www-form-urlencoded
SCRIPT_FILENAME=./www/cgi-bin/test.py
```

---

## 17.4 POST body 전달

POST 요청이면 request body를 CGI의 stdin으로 전달해야 한다.

구조:

```text
server write
→ pipe
→ CGI stdin
```

---

## 17.5 CGI output 읽기

CGI의 stdout을 서버가 읽어야 한다.

구조:

```text
CGI stdout
→ pipe
→ server read
→ HTTP response
→ client
```

주의:

```text
CGI pipe도 non-blocking으로 관리해야 한다.
pipe read/write도 poll 대상이어야 한다.
CGI가 출력하지 않는다고 서버가 멈추면 안 된다.
```

---

## 17.6 chunked request와 CGI

요청이 chunked일 경우 서버는 chunk를 해제해야 한다.

CGI는 chunked 형식을 모른다고 생각해야 한다.

잘못된 전달:

```text
4\r\nWiki\r\n5\r\npedia\r\n0\r\n\r\n
```

올바른 전달:

```text
Wikipedia
```

---

## 17.7 CGI timeout

CGI가 무한 루프를 돌 수 있다.

예:

```python
while True:
    pass
```

이 경우 서버가 같이 멈추면 안 된다.

따라서 다음 처리가 필요하다.

```text
CGI 시작 시간 저장
일정 시간 초과 시 kill
504 Gateway Timeout 또는 500 응답
fd 정리
프로세스 waitpid 처리
```

---

# 18. 여러 포트 listen

서버는 여러 포트에서 동시에 listen할 수 있어야 한다.

예:

```nginx
server {
    listen 127.0.0.1:8080;
    root ./site1;
}

server {
    listen 127.0.0.1:8081;
    root ./site2;
}
```

이 경우 listen socket이 여러 개 생긴다.

```text
listen_fd_8080
listen_fd_8081
```

둘 다 poll 대상에 포함해야 한다.

```text
poll_fds:
  listen_fd_8080
  listen_fd_8081
  client_fd_1
  client_fd_2
```

---

# 19. Virtual Host

과제 문서상 virtual host는 필수 범위가 아니다.

Virtual host는 같은 IP와 포트에서 `Host` header에 따라 다른 사이트를 제공하는 기능이다.

예:

```http
Host: site-a.com
Host: site-b.com
```

필수는 아니므로 mandatory를 안정적으로 완성한 뒤 여유가 있을 때 고려하는 것이 좋다.

---

# 20. 추천 프로젝트 구조

아래는 구현할 때 권장되는 모듈 구조이다.

```text
src/
  main.cpp
  config/
    ConfigParser.cpp
    ServerConfig.cpp
    LocationConfig.cpp
  core/
    Server.cpp
    EventLoop.cpp
    Client.cpp
  http/
    HttpRequest.cpp
    HttpRequestParser.cpp
    HttpResponse.cpp
    HttpResponseBuilder.cpp
    Router.cpp
  handlers/
    StaticFileHandler.cpp
    UploadHandler.cpp
    DeleteHandler.cpp
    CgiHandler.cpp
    ErrorPageHandler.cpp
  utils/
    FileUtils.cpp
    StringUtils.cpp
    MimeTypes.cpp
    Logger.cpp

include/
  ...

config/
  default.conf

www/
  index.html
  upload.html
  errors/
    404.html
    500.html
  uploads/
  cgi-bin/
    hello.py
```

---

# 21. 주요 클래스 설계 예시

## 21.1 ConfigParser

역할:

```text
설정 파일 읽기
문법 검사
server block 파싱
location block 파싱
ServerConfig 객체 생성
```

필요 기능:

```cpp
class ConfigParser {
public:
    std::vector<ServerConfig> parse(const std::string& path);
};
```

---

## 21.2 ServerConfig

역할:

```text
하나의 server block 설정 저장
```

저장할 값:

```text
listen host
listen port
root
index
error pages
client max body size
location list
```

---

## 21.3 LocationConfig

역할:

```text
location별 설정 저장
```

저장할 값:

```text
path prefix
allowed methods
root
index
autoindex
redirect
upload on/off
upload store path
cgi extension mapping
```

---

## 21.4 EventLoop

역할:

```text
poll fd 목록 관리
이벤트 감시
listen/client/cgi fd 처리
```

필요 기능:

```text
addFd
removeFd
updateEvents
run
handleRead
handleWrite
handleAccept
```

---

## 21.5 Client

역할:

```text
클라이언트 하나의 상태 저장
```

저장할 값:

```text
fd
read buffer
write buffer
request parser
response
send offset
connection state
last active time
```

---

## 21.6 HttpRequestParser

역할:

```text
read buffer에서 HTTP 요청 파싱
```

상태 머신으로 만드는 것이 좋다.

```text
READING_HEADER
READING_BODY
COMPLETE
ERROR
```

---

## 21.7 HttpResponseBuilder

역할:

```text
status code, headers, body를 조합해 raw HTTP response 생성
```

---

## 21.8 Router

역할:

```text
요청 URI에 맞는 server/location 설정 찾기
가장 긴 prefix match 수행
실제 파일 경로 계산
```

---

## 21.9 CgiHandler

역할:

```text
CGI 실행 준비
환경변수 생성
pipe/fork/execve
CGI stdin/stdout 처리
timeout 관리
```

---

# 22. 구현 순서 추천

이 과제는 한 번에 다 구현하려고 하면 매우 복잡하다.

다음 순서로 진행하는 것이 좋다.

---

## 1단계: 최소 서버 실행

목표:

```text
8080 포트 listen
브라우저 접속 시 고정 문자열 응답
```

예상 응답:

```http
HTTP/1.1 200 OK
Content-Type: text/plain
Content-Length: 12

Hello World!
```

확인:

```bash
curl http://localhost:8080
```

---

## 2단계: poll 기반 다중 클라이언트 처리

목표:

```text
listen fd와 client fd를 poll로 관리
여러 클라이언트가 동시에 접속해도 서버가 멈추지 않음
```

확인:

```bash
curl http://localhost:8080 &
curl http://localhost:8080 &
curl http://localhost:8080 &
```

---

## 3단계: HTTP request parser

목표:

```text
method, uri, version, headers 파싱
Content-Length 기반 body 수신
```

확인:

```bash
printf 'GET / HTTP/1.1\r\nHost: localhost\r\n\r\n' | nc localhost 8080
```

---

## 4단계: 정적 파일 GET

목표:

```text
/index.html 요청 시 실제 파일 반환
Content-Type 설정
Content-Length 정확히 계산
```

확인:

```bash
curl -i http://localhost:8080/index.html
```

---

## 5단계: config parser

목표:

```text
listen, root, index, error_page, location 파싱
```

처음에는 단순 문법으로 시작하고, 이후 확장한다.

---

## 6단계: route matching

목표:

```text
URL에 맞는 location 선택
location별 root/index/methods 적용
```

---

## 7단계: error page와 status code

목표:

```text
404, 403, 405, 413, 500 등 정확히 처리
기본 에러 페이지 생성
custom error page 적용
```

---

## 8단계: autoindex

목표:

```text
디렉토리 요청 시 index 없으면 파일 목록 HTML 생성
```

---

## 9단계: POST body 처리

목표:

```text
Content-Length 기반 body 전체 수신
client_max_body_size 적용
```

---

## 10단계: 파일 업로드

목표:

```text
multipart/form-data 파싱
upload_store에 파일 저장
```

---

## 11단계: DELETE

목표:

```text
허용된 route에서 파일 삭제
적절한 status code 반환
```

---

## 12단계: CGI

목표:

```text
.py CGI 실행
GET query string 전달
POST body 전달
CGI output 응답화
CGI timeout 처리
```

---

## 13단계: chunked request

목표:

```text
Transfer-Encoding: chunked 파싱
un-chunk 후 body 처리
CGI에는 unchunked body 전달
```

---

## 14단계: stress test

목표:

```text
다수 클라이언트
느린 클라이언트
큰 body
비정상 요청
중간 연결 종료
CGI timeout
```

---

# 23. 테스트 방법

## 23.1 curl 테스트

기본 GET:

```bash
curl -i http://localhost:8080/
```

없는 파일:

```bash
curl -i http://localhost:8080/notfound.html
```

POST:

```bash
curl -i -X POST http://localhost:8080/upload -d 'hello=world'
```

파일 업로드:

```bash
curl -i -F "file=@test.txt" http://localhost:8080/upload
```

DELETE:

```bash
curl -i -X DELETE http://localhost:8080/uploads/test.txt
```

리다이렉션:

```bash
curl -i http://localhost:8080/old
```

---

## 23.2 telnet 또는 nc 테스트

직접 HTTP 요청 보내기:

```bash
nc localhost 8080
```

입력:

```http
GET / HTTP/1.1
Host: localhost

```

주의: 실제 HTTP에서는 줄바꿈이 `\r\n`이어야 한다.

정확히 테스트하려면 다음처럼 한다.

```bash
printf 'GET / HTTP/1.1\r\nHost: localhost\r\n\r\n' | nc localhost 8080
```

---

## 23.3 브라우저 테스트

브라우저에서 다음을 확인한다.

```text
HTML 페이지 표시
CSS 적용
이미지 로딩
업로드 form 동작
redirect 동작
404 페이지 표시
CGI 결과 표시
```

---

## 23.4 NGINX와 비교

과제에서는 NGINX와 header 및 동작을 비교해볼 수 있다고 한다.

비교하면 좋은 것:

```text
404 응답 형식
405 응답
directory 접근
redirect 응답
Content-Length
Connection 처리
Content-Type
```

단, HTTP 버전 차이로 인해 NGINX와 완전히 같을 필요는 없다.

---

## 23.5 stress test 아이디어

Python으로 간단한 동시 요청 테스트를 만들 수 있다.

테스트해야 할 상황:

```text
동시에 100개 요청
큰 파일 다운로드
큰 파일 업로드
느린 client
요청 header만 보내고 대기
body를 조금씩 보내기
연결 중간에 끊기
CGI 무한 루프
존재하지 않는 파일 반복 요청
```

---

# 24. 자주 터지는 문제와 해결 방향

## 24.1 서버가 멈춤

원인 후보:

```text
blocking read/write 발생
CGI pipe에서 blocking
send가 한 번에 끝난다고 가정
recv가 한 번에 전체 요청을 받는다고 가정
```

해결:

```text
모든 socket/pipe fd를 non-blocking으로 설정
poll readiness 확인 후 read/write
client별 buffer와 offset 관리
```

---

## 24.2 브라우저가 계속 로딩함

원인 후보:

```text
Content-Length가 틀림
header 끝 \r\n\r\n 누락
body 길이 부족
Connection 처리 오류
응답을 끝까지 send하지 못함
```

해결:

```text
응답 raw string 출력해서 확인
curl -i로 header 확인
Content-Length와 실제 body.size() 비교
send offset 관리
```

---

## 24.3 업로드 파일이 깨짐

원인 후보:

```text
multipart boundary 처리 오류
body 일부만 저장
binary 파일을 string 처리하면서 손상
Content-Length만큼 다 읽기 전에 처리
```

해결:

```text
body 전체 수신 완료 후 파싱
binary-safe하게 std::string size 기준 처리
boundary 시작/끝 정확히 제거
```

---

## 24.4 CGI가 멈춤

원인 후보:

```text
CGI stdin을 닫지 않음
CGI stdout read에서 blocking
CGI 무한 루프 timeout 없음
waitpid 처리 오류
```

해결:

```text
CGI stdin에 body 전달 후 close
CGI fd도 poll로 관리
timeout 구현
waitpid WNOHANG 사용
```

---

## 24.5 DELETE가 위험하게 동작함

원인 후보:

```text
path traversal 미처리
root 밖 파일 삭제 가능
디렉토리 삭제 시도
권한 체크 부족
```

해결:

```text
.. 포함 경로 차단
실제 경로가 root 내부인지 확인
일반 파일만 삭제
access/stat 확인
```

---

# 25. 보안상 주의할 점

## 25.1 path traversal 방지

다음 요청은 위험하다.

```http
GET /../../etc/passwd HTTP/1.1
DELETE /../../important.txt HTTP/1.1
```

최소한 다음을 처리해야 한다.

```text
.. 경로 차단
URL decode 후 다시 검사
root 밖 접근 차단
```

---

## 25.2 업로드 파일명 검증

위험한 파일명:

```text
../a.txt
../../.bashrc
/absolute/path.txt
```

업로드 파일명은 안전하게 정제해야 한다.

---

## 25.3 CGI 실행 경로 제한

CGI로 아무 파일이나 실행하면 안 된다.

설정된 location과 확장자에 해당하는 파일만 실행해야 한다.

---

# 26. README.md 요구사항

프로젝트 루트에는 `README.md`가 있어야 한다.

README는 영어로 작성해야 한다.

## 26.1 첫 줄

첫 줄은 반드시 italic 형식이어야 한다.

```md
*This project has been created as part of the 42 curriculum by <login1>[, <login2>[, <login3>[...]].*
```

예:

```md
*This project has been created as part of the 42 curriculum by dc4881.*
```

---

## 26.2 Description 섹션

포함할 내용:

```text
프로젝트가 무엇인지
목표가 무엇인지
지원 기능 요약
```

예:

```md
## Description

Webserv is a lightweight HTTP server implemented in C++98.
It supports static file serving, configurable routes, file upload,
CGI execution, redirection, custom error pages, and non-blocking I/O.
```

---

## 26.3 Instructions 섹션

포함할 내용:

```text
컴파일 방법
실행 방법
설정 파일 예시
테스트 방법
```

예:

```md
## Instructions

```bash
make
./webserv config/default.conf
```
```

---

## 26.4 Resources 섹션

포함할 내용:

```text
HTTP 관련 문서
NGINX 문서
poll/select 문서
CGI 문서
AI 사용 방식
```

예:

```md
## Resources

- RFC 1945 - HTTP/1.0
- RFC 2616 - HTTP/1.1
- MDN Web Docs - HTTP
- NGINX Documentation
- Linux man pages: socket, bind, listen, accept, poll

AI was used to clarify HTTP concepts, design test cases, and review documentation.
All generated suggestions were manually reviewed, tested, and rewritten by the team.
```

---

# 27. Bonus

Bonus는 mandatory가 완벽할 때만 평가된다.

## 27.1 Cookie와 Session

쿠키 응답 예:

```http
Set-Cookie: session_id=abc123; Path=/
```

브라우저는 이후 요청에 다음 header를 보낸다.

```http
Cookie: session_id=abc123
```

이를 이용하면 간단한 로그인 상태를 구현할 수 있다.

---

## 27.2 multiple CGI types

여러 CGI 타입을 지원하는 기능이다.

예:

```text
.py  → /usr/bin/python3
.php → /usr/bin/php-cgi
.cgi → 직접 실행
```

Mandatory에서는 최소 하나의 CGI 타입만 제대로 구현하는 것이 우선이다.

---

# 28. 평가 대비 질문 목록

평가에서 받을 수 있는 질문은 다음과 같다.

## 28.1 서버 구조 관련

```text
전체 서버 구조를 설명해보세요.
poll fd 목록은 어떻게 관리하나요?
listen socket과 client socket은 어떻게 구분하나요?
한 client의 요청이 느리면 다른 client에 영향이 없나요?
client disconnect는 어떻게 처리하나요?
```

## 28.2 HTTP 관련

```text
HTTP request line은 어떻게 파싱하나요?
header의 끝은 어떻게 판단하나요?
Content-Length는 어떻게 처리하나요?
chunked request는 어떻게 처리하나요?
status code는 어떤 기준으로 정하나요?
```

## 28.3 config 관련

```text
config 파일 문법은 어떻게 설계했나요?
server block과 location block은 어떻게 저장하나요?
route matching은 어떤 방식인가요?
가장 긴 prefix match를 구현했나요?
```

## 28.4 file 관련

```text
URL을 실제 파일 경로로 어떻게 변환하나요?
index 파일은 어떻게 처리하나요?
autoindex는 어떻게 생성하나요?
권한 없는 파일은 어떻게 처리하나요?
path traversal은 막았나요?
```

## 28.5 upload 관련

```text
multipart/form-data는 어떻게 파싱하나요?
boundary는 어떻게 찾나요?
업로드 파일명은 어떻게 정하나요?
body size 제한은 어디서 확인하나요?
```

## 28.6 CGI 관련

```text
CGI는 어떤 흐름으로 실행되나요?
pipe는 몇 개 사용하나요?
CGI stdin/stdout은 어떻게 연결하나요?
환경변수는 무엇을 넘기나요?
POST body는 어떻게 CGI에 전달하나요?
CGI timeout은 어떻게 처리하나요?
```

## 28.7 non-blocking 관련

```text
왜 non-blocking이 필요한가요?
read/write 전에 poll을 거치는 이유는 무엇인가요?
errno 기반 EAGAIN 처리가 금지된 이유는 무엇인가요?
send가 한 번에 끝나지 않으면 어떻게 하나요?
recv가 요청을 조각내서 받으면 어떻게 하나요?
```

---

# 29. 최종 체크리스트

## 29.1 기본 실행

- [ ] `make` 성공
- [ ] `make re` 성공
- [ ] `-Wall -Wextra -Werror` 경고 없음
- [ ] `-std=c++98` 컴파일 가능
- [ ] `./webserv config/default.conf` 실행 가능
- [ ] 잘못된 config에서 적절히 에러 처리

## 29.2 서버 안정성

- [ ] 서버가 crash하지 않음
- [ ] client disconnect 처리
- [ ] 잘못된 요청 처리
- [ ] 큰 요청 처리
- [ ] 요청 timeout 처리
- [ ] CGI timeout 처리

## 29.3 HTTP

- [ ] GET 동작
- [ ] POST 동작
- [ ] DELETE 동작
- [ ] status code 정확성
- [ ] Content-Length 정확성
- [ ] Content-Type 설정
- [ ] 기본 에러 페이지
- [ ] custom error page

## 29.4 Config

- [ ] listen host:port
- [ ] multiple ports
- [ ] root
- [ ] index
- [ ] error_page
- [ ] client_max_body_size
- [ ] methods
- [ ] redirection
- [ ] autoindex
- [ ] upload_store
- [ ] CGI extension

## 29.5 Static Website

- [ ] HTML 제공
- [ ] CSS 제공
- [ ] JS 제공
- [ ] 이미지 제공
- [ ] favicon 처리
- [ ] 디렉토리 요청 처리

## 29.6 Upload

- [ ] upload route 동작
- [ ] multipart/form-data 파싱
- [ ] 파일 저장
- [ ] body size 제한
- [ ] 잘못된 filename 처리

## 29.7 CGI

- [ ] GET CGI 실행
- [ ] POST CGI 실행
- [ ] query string 전달
- [ ] body 전달
- [ ] 환경변수 설정
- [ ] CGI output 응답
- [ ] timeout 처리

## 29.8 테스트

- [ ] curl 테스트
- [ ] nc/telnet 테스트
- [ ] 브라우저 테스트
- [ ] NGINX 비교
- [ ] stress test

---

# 30. 핵심 결론

Webserv 과제는 단순한 HTTP 서버 구현 과제가 아니다.

이 과제의 진짜 목표는 다음이다.

```text
1. HTTP 요청과 응답의 실제 구조 이해
2. 소켓 프로그래밍 이해
3. non-blocking 이벤트 루프 설계
4. 여러 클라이언트 동시 처리
5. 설정 파일 기반 서버 동작 제어
6. 정적 파일, 업로드, DELETE, CGI 처리
7. 서버가 절대 멈추거나 죽지 않게 만드는 안정성 확보
```

가장 중요한 구현 원칙은 다음이다.

```text
모든 네트워크 I/O는 poll 또는 equivalent를 통해 readiness를 확인한 뒤 수행한다.
요청과 응답은 한 번에 끝난다고 가정하지 않는다.
클라이언트별 상태와 buffer를 반드시 관리한다.
CGI와 pipe도 blocking되지 않도록 관리한다.
에러 상황에서도 서버는 계속 살아 있어야 한다.
```

추천 구현 우선순위는 다음이다.

```text
1. poll 기반 서버 skeleton
2. HTTP request parser
3. GET 정적 파일 응답
4. config parser
5. route matching
6. error page / status code
7. POST body
8. upload
9. DELETE
10. CGI
11. chunked request
12. stress test
```

이 순서대로 구현하면 구조가 무너지지 않고, 평가에서 설명하기도 쉬워진다.

---

## Appendix A. 최소 HTTP 응답 예시

```http
HTTP/1.1 200 OK
Content-Type: text/plain
Content-Length: 12
Connection: close

Hello World!
```

---

## Appendix B. 최소 GET 요청 예시

```http
GET / HTTP/1.1
Host: localhost:8080

```

---

## Appendix C. 최소 POST 요청 예시

```http
POST /upload HTTP/1.1
Host: localhost:8080
Content-Length: 11
Content-Type: application/x-www-form-urlencoded

hello=world
```

---

## Appendix D. 최소 DELETE 요청 예시

```http
DELETE /uploads/test.txt HTTP/1.1
Host: localhost:8080

```

---

## Appendix E. 추천 디버깅 출력

개발 중에는 다음 정보를 로그로 찍으면 좋다.

```text
[ACCEPT] new client fd=5
[READ] fd=5 bytes=128
[REQUEST] method=GET uri=/index.html
[ROUTE] matched location=/ root=./www
[RESPONSE] status=200 length=1234
[WRITE] fd=5 sent=512/1234
[CLOSE] fd=5
[CGI] start pid=1234 script=hello.py
[CGI] timeout pid=1234
[ERROR] status=404 path=./www/notfound.html
```

단, 평가 제출 시 너무 과한 로그는 정리하는 것이 좋다.
