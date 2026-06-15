# SETTING.md

# 42 Webserv 설정 파일 요구사항 정리

## 0. 문서 목적

이 문서는 `42 webserv` 과제에서 중요한 부분인 **설정 파일(config file)** 요구사항을 정리한 문서이다.

`webserv`에서 설정 파일은 단순한 옵션 파일이 아니라, 서버가 다음을 결정하는 기준이 된다.

- 어떤 IP와 포트에서 요청을 받을지
- 어떤 URL 경로를 어떤 디렉토리에 연결할지
- 어떤 HTTP method를 허용할지
- 에러가 발생했을 때 어떤 페이지를 보여줄지
- 업로드를 허용할지
- CGI를 실행할지
- 디렉토리 listing을 보여줄지
- request body 크기를 어디까지 허용할지

즉, 설정 파일은 **서버 동작의 설계도**이다.

---

# 1. 설정 파일의 기본 역할

## 1.1 설정 파일을 사용하는 이유

서버 프로그램 안에 포트, root 경로, 에러 페이지, 업로드 경로 등을 하드코딩하면 유연성이 떨어진다.

예를 들어 코드에 직접 이렇게 작성하면 좋지 않다.

```cpp
port = 8080;
root = "./www";
max_body_size = 10000000;
```

이렇게 작성하면 서버 동작을 바꾸기 위해 매번 코드를 수정하고 다시 컴파일해야 한다.

대신 설정 파일을 사용하면 다음처럼 외부 파일만 수정해서 서버 동작을 바꿀 수 있다.

```conf
listen 127.0.0.1:8080;
root ./www;
client_max_body_size 10M;
```

즉, 같은 실행 파일이라도 설정 파일만 다르게 주면 서로 다른 서버처럼 동작할 수 있다.

```bash
./webserv config/default.conf
./webserv config/test.conf
./webserv config/site_a.conf
```

---

## 1.2 실행 방식

과제에서는 프로그램이 다음 중 하나의 방식으로 설정 파일을 가져야 한다.

### 방식 1. 명령행 인자로 설정 파일 받기

```bash
./webserv config/default.conf
```

이 경우 `argv[1]`에 들어온 경로를 설정 파일 경로로 사용한다.

### 방식 2. 기본 경로의 설정 파일 사용

```bash
./webserv
```

인자가 없을 경우 기본 설정 파일을 사용하게 만들 수 있다.

예:

```text
config/default.conf
```

---

## 1.3 설정 파일 처리 흐름

서버가 실행될 때 전체 흐름은 다음과 같다.

```text
main()
  ↓
config 파일 경로 결정
  ↓
config 파일 열기
  ↓
config 파일 문법 파싱
  ↓
ServerConfig / LocationConfig 구조체 생성
  ↓
listen socket 생성
  ↓
poll / select / kqueue / epoll에 listen socket 등록
  ↓
event loop 시작
```

설정 파일은 서버 실행 초기에 한 번 읽고, 그 결과를 내부 자료구조에 저장해둔다.

---

# 2. 전체 설정 파일 예시

아래는 webserv 과제에서 목표로 삼을 수 있는 설정 파일 예시이다.

```conf
server {
    listen 127.0.0.1:8080;
    root ./www/site_a;
    index index.html;
    client_max_body_size 10M;

    error_page 404 /errors/404.html;
    error_page 500 /errors/500.html;

    location / {
        methods GET;
        autoindex off;
        index index.html;
    }

    location /images {
        root ./www/images;
        methods GET;
        autoindex on;
    }

    location /upload {
        methods GET POST;
        upload on;
        upload_store ./www/uploads;
    }

    location /old {
        return 301 /new;
    }

    location /cgi-bin {
        methods GET POST;
        root ./www/cgi-bin;
        cgi .py /usr/bin/python3;
    }
}

server {
    listen 0.0.0.0:8081;
    root ./www/site_b;
    index index.html;
}
```

이 설정은 대략 다음 의미를 가진다.

```text
127.0.0.1:8080 → site_a 서버
0.0.0.0:8081   → site_b 서버

/images  → 이미지 디렉토리
/upload  → 업로드 처리
/old     → /new로 리다이렉트
/cgi-bin → CGI 실행
```

---

# 3. listen interface:port 설정

## 3.1 listen의 의미

`listen`은 서버가 어떤 주소와 포트에서 요청을 받을지 정하는 설정이다.

예:

```conf
listen 127.0.0.1:8080;
listen 0.0.0.0:8081;
```

각각의 의미는 다음과 같다.

```text
127.0.0.1:8080
→ 내 컴퓨터 내부에서만 접근 가능한 8080 포트

0.0.0.0:8081
→ 가능한 모든 네트워크 인터페이스에서 8081 포트 접근 허용
```

---

## 3.2 127.0.0.1과 0.0.0.0 차이

### 127.0.0.1

```conf
listen 127.0.0.1:8080;
```

`127.0.0.1`은 localhost를 의미한다.

브라우저에서 다음처럼 접근할 수 있다.

```text
http://127.0.0.1:8080
http://localhost:8080
```

하지만 외부 컴퓨터에서는 접근할 수 없다.

---

### 0.0.0.0

```conf
listen 0.0.0.0:8081;
```

`0.0.0.0`은 모든 인터페이스에서 요청을 받겠다는 의미이다.

즉, 다음 경로를 모두 허용할 수 있다.

```text
http://localhost:8081
http://127.0.0.1:8081
http://내_IP주소:8081
```

실제 네트워크에서 외부 접속을 허용할 때 사용된다.

---

## 3.3 여러 포트를 동시에 listen해야 한다

과제에서는 하나의 webserv 프로그램이 여러 개의 interface:port pair를 처리할 수 있어야 한다.

예:

```conf
server {
    listen 127.0.0.1:8080;
    root ./www/site_a;
}

server {
    listen 127.0.0.1:8081;
    root ./www/site_b;
}
```

실행은 한 번만 한다.

```bash
./webserv config/default.conf
```

하지만 내부적으로는 다음처럼 동작해야 한다.

```text
http://127.0.0.1:8080 → ./www/site_a
http://127.0.0.1:8081 → ./www/site_b
```

즉, 하나의 프로그램이 여러 개의 포트를 동시에 열어야 한다.

---

## 3.4 구현 관점

`listen` 설정마다 socket을 생성해야 한다.

```text
8080용 listen socket 생성
8081용 listen socket 생성
두 socket을 모두 poll/select/kqueue/epoll에 등록
이벤트가 발생한 socket을 확인
그 socket에 연결된 ServerConfig를 선택
```

예상 자료구조는 다음과 같다.

```cpp
struct ListenConfig {
    std::string host;
    int port;
};

struct ServerConfig {
    std::vector<ListenConfig> listens;
    std::string root;
    std::map<int, std::string> error_pages;
    size_t client_max_body_size;
    std::vector<LocationConfig> locations;
};
```

listen socket과 server config를 연결하기 위해 다음과 같은 map도 사용할 수 있다.

```cpp
std::map<int, ServerConfig*> listen_fd_to_server;
```

예:

```text
fd 3 → 127.0.0.1:8080 server config
fd 4 → 127.0.0.1:8081 server config
```

---

# 4. 기본 에러 페이지 설정

## 4.1 error_page의 의미

`error_page`는 특정 HTTP 에러가 발생했을 때 보여줄 HTML 파일을 지정하는 설정이다.

예:

```conf
error_page 404 /errors/404.html;
error_page 500 /errors/500.html;
```

이 설정은 다음 의미이다.

```text
404 에러 발생 → /errors/404.html 응답
500 에러 발생 → /errors/500.html 응답
```

---

## 4.2 예시

설정 파일:

```conf
root ./www;
error_page 404 /errors/404.html;
```

요청:

```http
GET /not_found.html HTTP/1.1
```

만약 `not_found.html` 파일이 없다면 서버는 404를 발생시킨다.

이때 설정에 따라 다음 파일을 찾는다.

```text
./www/errors/404.html
```

파일이 있으면 이 HTML 파일을 body로 사용한다.

응답 예시:

```http
HTTP/1.1 404 Not Found
Content-Type: text/html
Content-Length: ...

<html>
<body>
<h1>Custom 404 Page</h1>
</body>
</html>
```

---

## 4.3 에러 페이지 파일이 없을 때

설정에는 에러 페이지가 지정되어 있지만 실제 파일이 없을 수 있다.

예:

```conf
error_page 404 /errors/404.html;
```

하지만 실제로는 다음 파일이 존재하지 않을 수 있다.

```text
./www/errors/404.html
```

이 경우 다시 404 처리를 반복하면 무한 루프가 발생할 수 있다.

따라서 에러 페이지 파일을 읽지 못하면 서버가 기본 에러 페이지를 직접 만들어서 보내야 한다.

예:

```html
<html>
<body>
<h1>404 Not Found</h1>
</body>
</html>
```

---

## 4.4 에러 처리 흐름

```text
에러 발생
  ↓
해당 status code의 error_page 설정 확인
  ↓
설정이 있으면 파일 경로 계산
  ↓
파일 읽기 시도
  ↓
읽기 성공 → 해당 파일을 응답 body로 사용
  ↓
읽기 실패 → 기본 에러 HTML 생성
```

---

# 5. client_max_body_size

## 5.1 의미

`client_max_body_size`는 클라이언트가 보낼 수 있는 HTTP request body의 최대 크기를 제한하는 설정이다.

예:

```conf
client_max_body_size 10M;
```

이 설정은 request body를 최대 10MB까지만 허용한다는 뜻이다.

---

## 5.2 왜 필요한가?

이 제한이 없으면 클라이언트가 매우 큰 데이터를 보내 서버 메모리를 고갈시킬 수 있다.

예:

```text
클라이언트가 1GB 파일 업로드 시도
서버가 body를 계속 메모리에 저장
메모리 부족
서버 다운
```

따라서 서버는 body를 받기 전에 또는 받는 도중에 크기 제한을 검사해야 한다.

---

## 5.3 413 Payload Too Large

예:

```conf
client_max_body_size 10M;
```

클라이언트 요청:

```http
POST /upload HTTP/1.1
Content-Length: 20971520
```

20MB body를 보내려고 한다.

하지만 서버 제한은 10MB이다.

이 경우 응답은 다음과 같아야 한다.

```http
HTTP/1.1 413 Payload Too Large
Content-Type: text/html
Connection: close

<h1>413 Payload Too Large</h1>
```

---

## 5.4 Content-Length 검사

요청 헤더에 `Content-Length`가 있다면 body를 다 읽기 전에 미리 거절할 수 있다.

```text
Content-Length: 20000000
client_max_body_size: 10000000
```

이 경우:

```text
20000000 > 10000000
→ body 읽기 전 413 응답 가능
```

하지만 `Content-Length`만 믿으면 안 된다.

서버는 실제 body를 읽는 중에도 누적 크기를 확인해야 한다.

```text
read()로 body 일부 수신
  ↓
현재까지 받은 body 크기 누적
  ↓
client_max_body_size 초과 여부 확인
  ↓
초과하면 413 응답 생성
```

---

## 5.5 크기 단위 파싱

설정 파일에서는 다음과 같은 단위를 허용할 수 있다.

```conf
client_max_body_size 1000;
client_max_body_size 10K;
client_max_body_size 10M;
client_max_body_size 1G;
```

내부에서는 바이트 단위로 변환해서 저장하는 것이 좋다.

```text
1000 = 1000 bytes
10K  = 10 * 1024 bytes
10M  = 10 * 1024 * 1024 bytes
1G   = 1 * 1024 * 1024 * 1024 bytes
```

예상 함수:

```cpp
size_t parseBodySize(const std::string& value)
{
    // "10M" → 10485760
    // "1G"  → 1073741824
    // "100" → 100
}
```

---

# 6. location, 즉 route별 설정

## 6.1 location의 의미

`location`은 URL 경로별로 다르게 동작하도록 만드는 설정이다.

예:

```conf
location /images {
    root ./www/images;
    methods GET;
    autoindex on;
}
```

이 설정은 다음 의미이다.

```text
/images로 시작하는 요청은
./www/images 디렉토리를 기준으로 파일을 찾고,
GET method만 허용하고,
디렉토리 목록 출력을 허용한다.
```

---

## 6.2 요청 예시

요청:

```http
GET /images/cat.png HTTP/1.1
```

설정:

```conf
location /images {
    root ./www/images;
    methods GET;
    autoindex on;
}
```

서버는 `/images` location을 선택하고 실제 파일을 찾는다.

```text
./www/images/cat.png
```

---

## 6.3 location 매칭 방식

여러 location이 있을 수 있다.

```conf
location / {
    root ./www;
}

location /images {
    root ./www/images;
}

location /images/icons {
    root ./www/icons;
}
```

요청:

```http
GET /images/icons/home.png HTTP/1.1
```

이 요청은 다음 location과 모두 매칭된다.

```text
/
 /images
 /images/icons
```

이때 가장 구체적인 location을 선택하는 것이 좋다.

```text
/images/icons 선택
```

이를 일반적으로 **longest prefix match**라고 한다.

---

## 6.4 longest prefix match

location 선택 규칙은 다음처럼 잡으면 된다.

```text
요청 URI와 prefix가 일치하는 location을 모두 찾는다.
그중 prefix 길이가 가장 긴 location을 선택한다.
```

예:

```text
URI: /images/icons/home.png

location /              → match
location /images        → match
location /images/icons  → match

선택: /images/icons
```

---

# 7. 허용 method 목록

## 7.1 methods 설정

각 route마다 허용되는 HTTP method를 지정할 수 있어야 한다.

예:

```conf
location /upload {
    methods GET POST;
}
```

이 설정은 `/upload` 경로에서 `GET`과 `POST`만 허용한다는 의미이다.

```text
GET /upload    → 허용
POST /upload   → 허용
DELETE /upload → 거부
```

---

## 7.2 405 Method Not Allowed

허용되지 않은 method가 들어오면 `405 Method Not Allowed` 응답을 보내는 것이 적절하다.

예:

```http
DELETE /upload HTTP/1.1
```

설정:

```conf
location /upload {
    methods GET POST;
}
```

응답:

```http
HTTP/1.1 405 Method Not Allowed
Allow: GET, POST
Content-Type: text/html

<h1>405 Method Not Allowed</h1>
```

`Allow` 헤더에는 해당 route에서 허용되는 method 목록을 넣어주는 것이 좋다.

---

## 7.3 구현 흐름

```text
요청 파싱
  ↓
method 확인
  ↓
URI에 맞는 location 찾기
  ↓
location의 allowed methods 확인
  ↓
허용되지 않은 method면 405
  ↓
허용되면 다음 처리 진행
```

예상 자료구조:

```cpp
struct LocationConfig {
    std::string path;
    std::set<std::string> methods;
};
```

---

## 7.4 과제에서 주로 처리할 method

webserv 과제에서 일반적으로 핵심이 되는 method는 다음이다.

```text
GET
POST
DELETE
```

각 method의 역할은 다음과 같다.

| Method | 역할 |
|---|---|
| GET | 파일 조회, 정적 페이지 응답 |
| POST | body 전송, 업로드, CGI 입력 |
| DELETE | 파일 삭제 요청 처리 |

---

# 8. HTTP redirection

## 8.1 return 설정

특정 URL을 다른 URL로 리다이렉트할 수 있어야 한다.

예:

```conf
location /old {
    return 301 /new;
}
```

이 설정은 `/old`로 들어온 요청을 `/new`로 이동시키라는 의미이다.

---

## 8.2 요청과 응답 예시

요청:

```http
GET /old HTTP/1.1
Host: localhost:8080
```

응답:

```http
HTTP/1.1 301 Moved Permanently
Location: /new
Content-Length: 0
```

브라우저는 `Location` 헤더를 보고 자동으로 `/new`에 다시 요청을 보낸다.

---

## 8.3 자주 쓰는 redirect status code

| Status Code | 의미 |
|---|---|
| 301 | Moved Permanently, 영구 이동 |
| 302 | Found, 임시 이동 |
| 307 | Temporary Redirect, 임시 이동, method 유지 |
| 308 | Permanent Redirect, 영구 이동, method 유지 |

과제에서는 일반적으로 `301` 또는 `302` 처리를 구현하면 충분한 경우가 많다.

---

## 8.4 redirect가 있으면 즉시 응답한다

redirect 설정이 있는 location은 파일을 찾거나 CGI를 실행할 필요가 없다.

예:

```conf
location /old {
    return 301 /new;
    root ./www/old;
    methods GET;
}
```

요청:

```http
GET /old HTTP/1.1
```

처리 흐름:

```text
/old location 매칭
  ↓
return 설정 확인
  ↓
301 응답 생성
  ↓
root, index, autoindex, CGI 처리하지 않음
```

즉, redirect는 route 처리에서 우선순위가 높다.

---

# 9. root directory 설정

## 9.1 root의 의미

`root`는 URL을 실제 파일 시스템 경로로 바꾸기 위한 기준 디렉토리이다.

예:

```conf
location /images {
    root ./www/images;
}
```

요청:

```http
GET /images/cat.png HTTP/1.1
```

실제 파일 경로:

```text
./www/images/cat.png
```

---

## 9.2 과제 문서의 중요한 예시

과제 문서에서 중요한 예시는 다음과 같은 의미이다.

```text
URL /kapouet 이 /tmp/www에 root되어 있다.
요청: /kapouet/pouic/toto/pouet
실제 파일: /tmp/www/pouic/toto/pouet
```

즉, location prefix인 `/kapouet`을 제거하고 나머지 경로를 root에 붙여야 한다.

---

## 9.3 경로 변환 과정

요청 URI:

```text
/kapouet/pouic/toto/pouet
```

location prefix:

```text
/kapouet
```

prefix 제거 후:

```text
/pouic/toto/pouet
```

root:

```text
/tmp/www
```

최종 파일 경로:

```text
/tmp/www/pouic/toto/pouet
```

잘못된 방식은 다음과 같다.

```text
/tmp/www/kapouet/pouic/toto/pouet
```

위 방식은 URI 전체를 root 뒤에 붙였기 때문에 과제 요구와 맞지 않을 수 있다.

---

## 9.4 구현 예시

```cpp
std::string makeFilePath(const std::string& uri, const LocationConfig& loc)
{
    std::string relative = uri.substr(loc.path.length());

    if (relative.empty())
        relative = "/";

    return loc.root + relative;
}
```

예:

```text
uri = "/kapouet/pouic/toto/pouet"
loc.path = "/kapouet"
loc.root = "/tmp/www"

relative = "/pouic/toto/pouet"
result = "/tmp/www/pouic/toto/pouet"
```

---

# 10. directory listing, autoindex

## 10.1 autoindex의 의미

디렉토리를 요청했을 때 index 파일이 없으면, 디렉토리 목록을 보여줄지 말지 결정하는 설정이다.

예:

```conf
location /images {
    root ./www/images;
    autoindex on;
}
```

---

## 10.2 autoindex on

실제 디렉토리 구조:

```text
./www/images/
    cat.png
    dog.png
    icon.svg
```

요청:

```http
GET /images/ HTTP/1.1
```

만약 `index.html`이 없다면, `autoindex on`일 때 서버는 직접 HTML을 생성해야 한다.

응답 body 예시:

```html
<html>
<body>
<h1>Index of /images/</h1>
<a href="cat.png">cat.png</a><br>
<a href="dog.png">dog.png</a><br>
<a href="icon.svg">icon.svg</a><br>
</body>
</html>
```

---

## 10.3 autoindex off

설정:

```conf
autoindex off;
```

요청:

```http
GET /images/ HTTP/1.1
```

상황:

```text
/images/는 디렉토리
index.html 없음
autoindex off
```

이 경우 디렉토리 내부 목록을 보여주면 안 된다.

응답:

```http
HTTP/1.1 403 Forbidden
Content-Type: text/html

<h1>403 Forbidden</h1>
```

---

## 10.4 directory 요청 처리 순서

디렉토리 요청이 들어왔을 때 처리 순서는 다음과 같다.

```text
요청 URI를 실제 파일 경로로 변환
  ↓
그 경로가 디렉토리인지 확인
  ↓
index 설정이 있는지 확인
  ↓
index 파일이 실제로 존재하면 index 파일 응답
  ↓
index 파일이 없고 autoindex on이면 디렉토리 목록 HTML 생성
  ↓
index 파일이 없고 autoindex off이면 403 Forbidden
```

---

# 11. default index file

## 11.1 index의 의미

`index`는 디렉토리를 요청했을 때 기본으로 제공할 파일을 의미한다.

예:

```conf
index index.html;
```

요청:

```http
GET / HTTP/1.1
```

실제 경로:

```text
./www/
```

디렉토리라면 서버는 다음 파일을 찾는다.

```text
./www/index.html
```

파일이 있으면 해당 파일을 응답한다.

---

## 11.2 여러 index 파일

여러 개의 index 파일을 허용할 수도 있다.

```conf
index index.html index.htm home.html;
```

이 경우 순서대로 파일을 찾는다.

```text
./www/index.html 있음? → 있으면 응답
없으면 ./www/index.htm 확인
없으면 ./www/home.html 확인
없으면 autoindex 확인
```

---

## 11.3 index와 autoindex의 우선순위

중요한 우선순위는 다음과 같다.

```text
1순위: index 파일
2순위: autoindex on이면 디렉토리 목록
3순위: autoindex off이면 403 Forbidden
```

따라서 `autoindex on`이어도 `index.html`이 있으면 디렉토리 목록을 보여주는 것이 아니라 `index.html`을 응답하는 것이 자연스럽다.

---

# 12. 업로드 허용 및 저장 위치

## 12.1 upload 설정

route마다 업로드 허용 여부와 저장 위치를 정할 수 있어야 한다.

예:

```conf
location /upload {
    methods GET POST;
    upload on;
    upload_store ./www/uploads;
}
```

이 설정은 다음 의미이다.

```text
/upload route에서는
GET, POST를 허용하고
POST 요청으로 업로드를 받을 수 있으며
업로드된 파일은 ./www/uploads에 저장한다.
```

---

## 12.2 업로드 요청 예시

클라이언트 요청:

```http
POST /upload HTTP/1.1
Host: localhost:8080
Content-Length: 1024
Content-Type: application/octet-stream

...파일 데이터...
```

서버는 body를 읽고 파일로 저장한다.

예:

```text
./www/uploads/upload_001.bin
```

`multipart/form-data`를 처리한다면 원래 파일명을 추출해서 저장할 수도 있다.

예:

```text
./www/uploads/cat.png
```

---

## 12.3 upload off일 때

설정:

```conf
location /upload {
    methods GET POST;
    upload off;
}
```

이 상태에서 클라이언트가 POST body를 업로드하려고 하면 거절해야 한다.

가능한 응답:

```text
403 Forbidden
```

`POST` method 자체가 허용되어 있는데 업로드 행위만 금지된 경우라면 `405 Method Not Allowed`보다는 `403 Forbidden`이 더 자연스럽다.

```text
POST method는 허용됨
하지만 이 route에서 업로드는 금지됨
→ 403 Forbidden
```

---

## 12.4 업로드와 client_max_body_size

업로드는 반드시 `client_max_body_size`와 함께 처리해야 한다.

설정:

```conf
client_max_body_size 10M;

location /upload {
    methods GET POST;
    upload on;
    upload_store ./www/uploads;
}
```

요청:

```http
POST /upload HTTP/1.1
Content-Length: 20971520
```

판단:

```text
/upload location 매칭
POST 허용
upload on
하지만 body 크기가 20MB
client_max_body_size는 10MB
→ 413 Payload Too Large
```

---

## 12.5 upload_store 검증

설정:

```conf
upload_store ./www/uploads;
```

서버는 업로드 전에 다음을 확인하는 것이 좋다.

```text
./www/uploads가 존재하는가?
디렉토리인가?
쓰기 권한이 있는가?
```

문제가 있다면 다음 응답을 고려할 수 있다.

```text
403 Forbidden
500 Internal Server Error
```

예를 들어 경로는 존재하지만 쓰기 권한이 없다면 `403 Forbidden`이 자연스럽다.

서버 내부 처리 실패라면 `500 Internal Server Error`를 사용할 수 있다.

---

# 13. CGI 실행 설정

## 13.1 CGI의 의미

CGI는 정적 파일을 그대로 보내는 것이 아니라, 특정 프로그램을 실행하고 그 출력 결과를 HTTP 응답으로 보내는 기능이다.

예:

```conf
location /cgi-bin {
    root ./www/cgi-bin;
    methods GET POST;
    cgi .py /usr/bin/python3;
}
```

요청:

```http
GET /cgi-bin/hello.py HTTP/1.1
```

서버는 `hello.py` 파일을 그대로 보내지 않는다.

대신 다음처럼 실행한다.

```bash
/usr/bin/python3 ./www/cgi-bin/hello.py
```

그리고 실행 결과를 HTTP 응답 body로 사용한다.

---

## 13.2 CGI script 예시

`hello.py`:

```python
print("Content-Type: text/html")
print()
print("<h1>Hello CGI</h1>")
```

서버 응답:

```http
HTTP/1.1 200 OK
Content-Type: text/html

<h1>Hello CGI</h1>
```

---

## 13.3 CGI 환경변수

CGI 실행 시 서버는 여러 환경변수를 넘겨줘야 한다.

예:

```text
REQUEST_METHOD=GET
SCRIPT_FILENAME=./www/cgi-bin/hello.py
QUERY_STRING=name=kyu
CONTENT_LENGTH=123
CONTENT_TYPE=application/x-www-form-urlencoded
PATH_INFO=
SERVER_PROTOCOL=HTTP/1.1
```

요청:

```http
GET /cgi-bin/hello.py?name=kyu HTTP/1.1
```

분리 결과:

```text
path: /cgi-bin/hello.py
query string: name=kyu
```

CGI 환경변수:

```text
QUERY_STRING=name=kyu
```

---

## 13.4 POST CGI

POST 요청의 경우 request body를 CGI 프로그램의 표준입력으로 전달해야 한다.

흐름:

```text
클라이언트 body
  ↓
서버가 body 읽음
  ↓
pipe로 CGI stdin에 전달
  ↓
CGI stdout을 읽음
  ↓
HTTP response body로 사용
```

구현 흐름:

```text
fork()
  ↓
child process:
    stdin을 pipe로 연결
    stdout을 pipe로 연결
    execve(CGI interpreter, script)
  ↓
parent process:
    request body를 child stdin에 씀
    child stdout을 읽음
    response 생성
```

---

# 14. 요청 처리 전체 흐름

클라이언트 요청 하나가 들어왔을 때 서버는 설정 파일을 기준으로 다음 순서로 처리하면 된다.

```text
1. 요청 파싱
   예: GET /images/cat.png HTTP/1.1

2. 어떤 listen socket으로 들어왔는지 확인
   예: 127.0.0.1:8080

3. 해당 server config 선택

4. URI에 맞는 location 선택
   예: /images

5. redirect 설정 확인
   있으면 즉시 301/302 응답

6. method 허용 여부 확인
   허용되지 않으면 405 Method Not Allowed

7. client_max_body_size 확인
   body가 너무 크면 413 Payload Too Large

8. CGI 대상인지 확인
   CGI면 CGI 실행

9. 실제 파일 경로 계산
   location prefix 제거 + root 결합

10. 파일/디렉토리 상태 확인
    파일이면 파일 응답
    디렉토리면 index 확인
    index 없으면 autoindex 확인

11. 에러 발생 시 error_page 확인

12. 최종 HTTP response 생성
```

---

# 15. 설정값 상속 구조

## 15.1 server 기본값과 location override

보통 `server` 블록에 기본 설정을 두고, `location`에서 필요한 설정만 덮어쓰는 구조가 좋다.

예:

```conf
server {
    root ./www;
    index index.html;
    client_max_body_size 10M;

    location /images {
        autoindex on;
    }

    location /upload {
        methods GET POST;
        upload on;
        upload_store ./www/uploads;
    }
}
```

이 경우 `/images` location에는 root가 없다.

따라서 server의 root를 상속받는다.

```text
/images route의 root 설정 없음
→ server root ./www 사용
```

---

## 15.2 location이 root를 가지면 location 설정 우선

예:

```conf
server {
    root ./www;

    location /images {
        root ./static/images;
    }
}
```

요청:

```http
GET /images/cat.png HTTP/1.1
```

결과:

```text
./static/images/cat.png
```

location에 root가 있으면 server root보다 location root가 우선한다.

---

# 16. 설정 파일 파싱 시 검증해야 하는 것

설정 파일은 서버 시작 전에 검증하는 것이 좋다.

잘못된 설정이 있으면 서버를 실행하지 말고 에러를 출력하고 종료해야 한다.

---

## 16.1 listen 검증

잘못된 예:

```conf
listen abc:hello;
```

문제:

```text
host 형식이 이상함
port가 숫자가 아님
```

검증할 것:

```text
port가 숫자인가?
port 범위가 1~65535인가?
host가 비어 있지 않은가?
```

---

## 16.2 client_max_body_size 검증

잘못된 예:

```conf
client_max_body_size -10M;
```

문제:

```text
음수 크기
```

검증할 것:

```text
숫자 부분이 올바른가?
단위가 허용되는가? K, M, G 등
최종 크기가 overflow되지 않는가?
```

---

## 16.3 upload 설정 검증

잘못된 예:

```conf
location /upload {
    upload on;
}
```

문제:

```text
upload는 켰지만 upload_store가 없음
```

검증할 것:

```text
upload on이면 upload_store가 있는가?
upload_store 경로가 비어 있지 않은가?
실행 시점에 해당 경로 접근 가능한가?
```

---

## 16.4 error_page 검증

잘못된 예:

```conf
error_page abc /404.html;
```

문제:

```text
status code가 숫자가 아님
```

검증할 것:

```text
status code가 숫자인가?
유효한 HTTP status code인가?
path가 비어 있지 않은가?
```

---

## 16.5 methods 검증

잘못된 예:

```conf
methods GET FLY;
```

문제:

```text
FLY는 지원하지 않는 method
```

과제에서 주로 허용할 method:

```text
GET
POST
DELETE
```

---

# 17. 내부 자료구조 예시

설정 파일을 파싱한 후에는 구조체로 정리해두는 것이 좋다.

```cpp
struct ListenConfig {
    std::string host;
    int port;
};

struct RedirectConfig {
    bool enabled;
    int status_code;
    std::string target;
};

struct CgiConfig {
    bool enabled;
    std::map<std::string, std::string> extension_to_path;
    // 예: ".py" -> "/usr/bin/python3"
};

struct LocationConfig {
    std::string path;

    std::set<std::string> methods;

    bool has_root;
    std::string root;

    bool autoindex;
    std::vector<std::string> index_files;

    bool upload_enabled;
    std::string upload_store;

    RedirectConfig redirect;
    CgiConfig cgi;
};

struct ServerConfig {
    std::vector<ListenConfig> listens;

    std::string root;
    std::vector<std::string> index_files;

    std::map<int, std::string> error_pages;

    size_t client_max_body_size;

    std::vector<LocationConfig> locations;
};
```

---

# 18. 설정별 역할 요약

| 설정 | 역할 | 에러 시 대표 응답 |
|---|---|---|
| `listen` | 서버가 열 주소와 포트 설정 | 설정 오류 시 서버 실행 실패 |
| `error_page` | 에러 발생 시 보여줄 HTML 지정 | 파일 없으면 기본 에러 페이지 |
| `client_max_body_size` | request body 최대 크기 제한 | `413 Payload Too Large` |
| `methods` | route별 허용 method 지정 | `405 Method Not Allowed` |
| `return` | HTTP redirection 처리 | `301`, `302` 등 |
| `root` | URL을 실제 파일 경로로 매핑 | 파일 없으면 `404 Not Found` |
| `autoindex` | 디렉토리 목록 출력 여부 | off이고 index 없으면 `403 Forbidden` |
| `index` | 디렉토리 요청 시 기본 파일 | 없으면 autoindex 판단 |
| `upload` | 업로드 허용 여부 | 금지 시 `403 Forbidden` |
| `upload_store` | 업로드 파일 저장 위치 | 실패 시 `500` 또는 `403` |
| `cgi` | 특정 확장자 파일 실행 | 실패 시 `500 Internal Server Error` |

---

# 19. 예시 상황별 판단

## 19.1 body 크기 초과

요청:

```http
POST /upload HTTP/1.1
Content-Length: 20971520
```

설정:

```conf
client_max_body_size 10M;

location /upload {
    methods GET POST;
    upload on;
    upload_store ./www/uploads;
}
```

판단:

```text
/upload location 매칭
POST 허용됨
upload on
하지만 body가 20MB
client_max_body_size는 10MB
→ 413 Payload Too Large
```

---

## 19.2 허용되지 않은 method

요청:

```http
DELETE /images/cat.png HTTP/1.1
```

설정:

```conf
location /images {
    methods GET;
    root ./www/images;
}
```

판단:

```text
/images location 매칭
DELETE는 허용되지 않음
→ 405 Method Not Allowed
```

---

## 19.3 디렉토리 요청과 autoindex

요청:

```http
GET /images/ HTTP/1.1
```

설정:

```conf
location /images {
    root ./www/images;
    methods GET;
    autoindex on;
    index index.html;
}
```

판단:

```text
/images location 매칭
/images/는 디렉토리
index.html 있으면 index.html 응답
index.html 없으면 autoindex HTML 생성
```

---

## 19.4 redirect

요청:

```http
GET /old HTTP/1.1
```

설정:

```conf
location /old {
    return 301 /new;
}
```

판단:

```text
/old location 매칭
return 301 /new 설정 존재
파일 탐색 없이 즉시 redirect 응답
→ 301 Moved Permanently
→ Location: /new
```

---

## 19.5 root path 변환

요청:

```http
GET /kapouet/pouic/toto/pouet HTTP/1.1
```

설정:

```conf
location /kapouet {
    root /tmp/www;
}
```

판단:

```text
location prefix: /kapouet
요청 URI: /kapouet/pouic/toto/pouet
prefix 제거: /pouic/toto/pouet
root와 결합: /tmp/www/pouic/toto/pouet
```

---

# 20. 구현 시 추천 처리 우선순위

요청을 처리할 때 추천하는 우선순위는 다음과 같다.

```text
1. server 선택
2. location 선택
3. redirect 확인
4. method 검사
5. body size 검사
6. upload / CGI 여부 확인
7. 실제 파일 경로 계산
8. 파일 존재 여부 확인
9. 디렉토리라면 index 확인
10. index 없으면 autoindex 확인
11. 에러 발생 시 error_page 확인
12. response 생성
```

redirect는 파일 처리보다 먼저 확인하는 것이 좋다.

method 검사는 body나 파일을 처리하기 전에 먼저 하는 것이 좋다.

body size 검사는 body를 메모리에 많이 쌓기 전에 해야 한다.

---

# 21. 핵심 정리

webserv 설정 파일에서 반드시 지원해야 할 핵심 기능은 다음이다.

```text
1. listen interface:port 설정
2. 여러 interface:port pair 처리
3. 기본 에러 페이지 설정
4. client request body 최대 크기 제한
5. route별 method 제한
6. route별 HTTP redirection
7. route별 root directory 설정
8. directory listing on/off
9. default index file 설정
10. upload 허용 여부 및 저장 위치 설정
11. CGI 실행 설정
```

이 중에서도 특히 중요한 것은 다음이다.

```text
요청 URI가 들어왔을 때
어떤 server config를 사용할지,
어떤 location config를 사용할지,
실제 파일 경로를 어떻게 계산할지,
method/body/upload/CGI/autoindex/error_page를 어떤 순서로 적용할지
명확하게 설계하는 것
```

---

# 22. 한 줄 요약

`webserv`의 설정 파일은 **listen할 포트, 에러 페이지, body 크기 제한, URL별 method/root/index/autoindex/upload/CGI/redirect 규칙을 정의하는 서버 동작 설계도**이고, 서버는 요청이 들어올 때마다 이 설정을 기준으로 어떤 응답을 만들지 결정해야 한다.
