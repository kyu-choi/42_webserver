*이 프로젝트는 42 커리큘럼의 일부로 hyeonwki가 작성했습니다*

# Webserv

## 설명

Webserv는 C++98로 작성한 작은 HTTP 서버입니다. 이 프로젝트의 목표는
웹 서버가 동작하는 방식을 직접 구현하면서 이해하는 것입니다. TCP 소켓,
HTTP 요청 파싱, 설정 파일 기반 라우팅, 정적 파일 제공, 파일 업로드,
DELETE 요청, CGI 실행, non-blocking I/O를 직접 다룹니다.

서버 설정 구조는 NGINX의 `server` 블록에서 영감을 받았지만, NGINX나
외부 HTTP 라이브러리를 사용하지 않습니다. HTTP 요청 파싱, 응답 생성,
라우팅, 설정 파일 파싱은 모두 프로젝트 내부에서 직접 구현했습니다.

실행 중 서버는 다음 흐름으로 동작합니다.

```text
TCP 연결을 받는다
HTTP 요청을 파싱한다
설정 파일에서 server와 location을 선택한다
정적 파일, 업로드, DELETE, redirect, directory listing, CGI 중 하나를 처리한다
서버 전체가 멈추지 않도록 HTTP 응답을 전송한다
```

## 기능

- C++98 구현
- 실행 파일 이름 `webserv`
- 설정 파일 지원
- 인자가 없을 때 기본 설정 경로 사용
- non-blocking socket과 CGI pipe
- listen socket, client, CGI pipe를 하나의 `poll()` event loop로 관리
- GET, POST, DELETE, HEAD 파싱
- 주요 에러 상황에 맞는 HTTP status code 응답
- 기본 에러 페이지와 설정된 custom error page 지원
- 정적 웹사이트 제공
- 디렉터리 index 파일 처리
- 설정에 따라 directory listing 제공
- HTTP redirect
- request body 크기 제한
- 파일 업로드
- 안전한 DELETE 처리
- path traversal 차단
- 파일 확장자 기반 CGI 실행
- chunked request body를 handler나 CGI에 넘기기 전에 decoding
- 여러 listen port에서 서로 다른 content 제공
- cookie/session 예제
- 여러 CGI interpreter 예제

## 실행 방법

프로젝트를 빌드합니다.

```sh
make
```

설정 파일을 명시해서 실행합니다.

```sh
./webserv config/default.conf
```

기본 설정 경로로 실행할 수도 있습니다.

```sh
./webserv
```

브라우저에서 다음 주소를 열어 확인합니다.

```text
http://127.0.0.1:8080/
```

두 번째 listen port까지 포함한 전체 시연 설정은 다음과 같이 실행합니다.

```sh
./webserv config/step20.conf
```

그 뒤 다음 주소를 열어 확인합니다.

```text
http://127.0.0.1:8080/
http://127.0.0.1:8081/
```

서버는 `Ctrl-C`로 종료합니다.

## Makefile

Makefile은 필수 rule을 제공합니다.

```sh
make
make all
make clean
make fclean
make re
```

프로젝트는 다음 플래그로 컴파일됩니다.

```sh
c++ -Wall -Wextra -Werror -std=c++98
```

외부 라이브러리와 Boost는 사용하지 않습니다.

## 설정 파일

설정 파일 형식은 NGINX에서 영감을 받았습니다. 기본 예시는 다음과 같습니다.

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

지원하는 directive는 다음과 같습니다.

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

## 시연용 Route

`config/step20.conf`를 사용하면 mandatory와 bonus 기능을 함께 확인할 수 있습니다.

- `/` 는 정적 웹사이트를 제공합니다.
- `/old` 는 `/` 로 redirect합니다.
- `/listing/` 은 directory listing을 보여줍니다.
- `/private-directory/` 는 listing이 꺼져 있을 때 에러를 반환합니다.
- `/has-index/` 는 설정된 index 파일을 제공합니다.
- `/echo` 는 POST body를 그대로 돌려주며 chunked body 테스트에 유용합니다.
- `/upload` 는 파일 업로드를 받습니다.
- `/delete/<file>` 은 업로드된 파일을 삭제합니다.
- `/cgi-bin/hello.py` 는 Python CGI를 실행합니다.
- `/cgi-bin/hello.sh` 는 shell CGI를 실행합니다.
- `/session` 은 cookie와 session 저장을 시연합니다.
- `8081` port는 다른 site root를 제공합니다.

## CGI

CGI 프로그램은 설정 파일의 확장자 매핑으로 선택됩니다. 서버는 pipe를 만들고,
CGI 실행을 위해서만 `fork()`를 사용합니다. request body는 CGI stdin으로
전달하고, CGI stdout을 읽어서 HTTP response로 변환합니다.

CGI 환경 변수에는 request method, URI, script path, query string,
content header, server 정보, remote address, HTTP request header가
포함됩니다. Chunked request body는 CGI에 전달되기 전에 decoding되며,
CGI 입력 body의 끝은 EOF로 표시됩니다.

## 테스트

전체 로컬 테스트를 실행합니다.

```sh
./tests/integration.sh
```

valgrind smoke check 없이 실행합니다.

```sh
SKIP_VALGRIND=1 ./tests/integration.sh
```

valgrind smoke test만 실행합니다.

```sh
python3 tests/valgrind_smoke.py
```

수동 확인에 유용한 명령어는 다음과 같습니다.

```sh
curl -i http://127.0.0.1:8080/
curl -i http://127.0.0.1:8081/
curl -i http://127.0.0.1:8080/no-such-file
curl -i http://127.0.0.1:8080/old
curl -i http://127.0.0.1:8080/listing/
curl -i "http://127.0.0.1:8080/cgi-bin/hello.py?name=eval"
curl --path-as-is -i http://127.0.0.1:8080/../../etc/passwd
```

repository에는 다음 내용을 확인하는 Python 테스트도 포함되어 있습니다.

- 설정 파일 파싱
- 잘못된 설정 파일 거절
- 정적 라우팅
- 업로드와 DELETE
- malformed request
- chunked request
- 느린 client와 연결 해제
- non-blocking CGI 동작
- 동시 요청 stress
- cookie/session bonus 동작
- 여러 CGI type

## 프로젝트 구조

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

주요 module은 다음과 같습니다.

- `ConfigParser`: 설정 파일을 파싱합니다.
- `Server`: non-blocking listen socket을 생성합니다.
- `EventLoop`: 하나의 `poll()` loop를 실행합니다.
- `RequestParser`: HTTP request line, header, body를 파싱합니다.
- `Router`: 유효한 server/location 설정을 선택합니다.
- `StaticFileHandler`: 파일, index, autoindex page를 제공합니다.
- `UploadHandler`: raw body와 multipart upload를 저장합니다.
- `DeleteHandler`: 파일을 안전하게 삭제합니다.
- `CgiHandler`: CGI process를 실행하고 CGI response를 만듭니다.
- `ResponseBuilder`: HTTP response를 생성합니다.
- `PathPolicy`: URI path를 정규화하고 traversal을 차단합니다.

## 참고 자료

- RFC 9110: HTTP Semantics
- RFC 9112: HTTP/1.1
- MDN Web Docs: HTTP overview, methods, status codes, headers
- Linux man pages: `socket`, `bind`, `listen`, `accept`, `poll`, `fcntl`,
  `recv`, `send`, `pipe`, `fork`, `execve`, `waitpid`
- NGINX documentation: server block, location matching, root, index,
  error page, client body size, CGI와 유사한 upstream 개념
- Python CGI documentation과 일반 CGI environment variable 참고 자료

AI는 이 프로젝트에서 학습과 검토 보조 도구로 사용되었습니다. Subject 요약,
HTTP/NGINX/설정 파일 관련 메모 정리, 테스트 체크리스트 구성, README 문장 개선,
수동 검증 케이스 제안에 활용했습니다. 구현 결정, 코드 변경, 최종 동작은 프로젝트
작성자가 직접 검토하고 테스트했습니다.
