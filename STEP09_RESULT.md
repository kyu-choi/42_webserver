# STEP 09 구현 결과

## 목표

`STEP09.md`의 목표는 하나의 `webserv` 프로세스가 여러 interface:port를 동시에 listen하고, 요청 URI에 맞는 location과 실제 파일 경로를 선택하게 만드는 것이다.

이번 단계에서는 다음 흐름을 구현했다.

```text
config parse
-> 모든 server/listen 조합으로 listen socket 생성
-> 모든 listen fd를 같은 poll loop에 등록
-> accept 시 client에 listen fd 저장
-> request 처리 시 listen fd로 ServerConfig 선택
-> Router가 longest-prefix location 선택
-> effective config 계산
-> 안전한 filesystem path로 정적 파일 응답
```

## 생성/수정한 파일

| 경로 | 구현 내용 |
|---|---|
| `include/webserv/Router.hpp` | Router 결과 구조와 effective config 선언 |
| `src/http/Router.cpp` | longest-prefix location 선택, effective config 계산, URI->filesystem path 변환 |
| `include/webserv/Server.hpp` | 여러 server/listen fd를 저장하는 구조로 변경 |
| `src/core/Server.cpp` | 모든 listen socket 생성, 중복 listen endpoint 검사, EventLoop에 fd별 config 전달 |
| `include/webserv/EventLoop.hpp` | 여러 listen fd와 fd별 ServerConfig를 받도록 변경 |
| `src/core/EventLoop.cpp` | listen fd별 accept, client listen fd 기반 routing 연결 |
| `include/webserv/StaticFileHandler.hpp` | Router가 계산한 path를 직접 처리하는 API 추가 |
| `src/handlers/StaticFileHandler.cpp` | directory index 탐색과 resolved path 응답 추가 |
| `src/main.cpp` | 첫 번째 server만 실행하지 않고 전체 ServerConfig 목록을 Server에 전달 |
| `Makefile` | `Router.cpp` 빌드 대상 추가 |
| `config/routing.conf` | STEP09 검증용 multi-port/location config |
| `www/site_b/index.html` | 8081 전용 root 테스트 페이지 |
| `www/images/index.html` | `/images` location index 테스트 파일 |
| `www/icons/a.txt` | `/images/icons` longest-prefix 테스트 파일 |

## 여러 listen socket 구현

기존에는 `Server`가 `_listenFd` 하나만 가지고 있었다.

이번 단계에서는 `Server`가 모든 `ServerConfig`의 모든 `listen` 값을 순회해서 socket을 만든다.

```text
ServerConfig[0].listen[0] -> listen fd 3
ServerConfig[1].listen[0] -> listen fd 4
...
```

그리고 `EventLoop`에는 다음 관계를 넘긴다.

```text
listen fd -> ServerConfig
```

client를 accept할 때는 기존 `Client(fd, listenFd)` 구조를 활용해서 다음 관계가 유지된다.

```text
client fd -> accept된 listen fd -> ServerConfig
```

이 덕분에 8080으로 들어온 요청과 8081로 들어온 요청이 서로 다른 root를 사용할 수 있다.

## Router 구현

`Router::route()`는 request와 server config를 받아 `RouteResult`를 만든다.

Router가 하는 일:

```text
1. query string 제거
2. percent decoding
3. . / .. path segment 검사
4. normalized URI path 생성
5. longest-prefix location 선택
6. server + location effective config 계산
7. location prefix 제거
8. root + relative path 결합
```

예시:

```text
request URI: /images/icons/a.txt
matched location: /images/icons
location root: ./www/icons
relative path: a.txt
filesystem path: ./www/icons/a.txt
```

## longest-prefix location 선택

location matching은 단순 문자열 prefix가 아니라 segment 경계를 확인한다.

```text
/images/logo.svg  -> /images 매칭
/images/icons/a   -> /images/icons 매칭
/images-other     -> /images 매칭 아님
```

즉 `/images-other`는 `/images` location으로 잘못 들어가지 않는다.

## effective config 계산

location에서 명시한 값은 location 값을 사용하고, 명시하지 않은 값은 server 값을 사용한다.

현재 Router가 합치는 값:

```text
root
index 목록
methods
client_max_body_size
error_page
autoindex
upload/upload_store
redirect
cgi
```

현재 STEP09에서 실제 응답 처리에 바로 쓰는 값은 `root`, `index`, `methods`, `redirect`다. 나머지는 이후 upload/cgi/error-page/body 처리 단계에서 사용할 수 있도록 RouteResult에 보관한다.

## 정적 파일 처리 변경

기존 `StaticFileHandler`는 request URI와 root를 받아 내부에서 path를 계산했다.

이제는 Router가 이미 안전한 filesystem path를 계산하므로, `StaticFileHandler::handlePath()`가 그 path를 받아 파일을 읽는다.

directory 요청이면 config의 index 목록을 순서대로 검사한다.

```text
/images
-> ./www/images
-> ./www/images/index.html
```

## 추가한 테스트 config

`config/routing.conf`:

```conf
server {
    listen 127.0.0.1:8080;
    root ./www;

    location /images {
        root ./www/images;
    }

    location /images/icons {
        root ./www/icons;
    }
}

server {
    listen 127.0.0.1:8081;
    root ./www/site_b;
}
```

## 검증 결과

빌드:

```bash
make
```

결과:

```text
성공
```

서버 실행:

```bash
./webserv config/routing.conf
```

확인한 출력:

```text
parsed servers: 2
listen sockets: 2
webserv listening on 127.0.0.1:8080
webserv listening on 127.0.0.1:8081
```

HTTP 요청 검증:

```bash
curl -i http://127.0.0.1:8080/
curl -i http://127.0.0.1:8081/
curl -i http://127.0.0.1:8080/images/logo.svg
curl -i http://127.0.0.1:8080/images/icons/a.txt
curl -i http://127.0.0.1:8080/images
curl -i http://127.0.0.1:8080/images/
curl -i http://127.0.0.1:8080/images-other
curl -i http://127.0.0.1:8080/%2e%2e/%2e%2e/etc/passwd
```

확인한 결과:

```text
8080 /                         -> 200 OK, ./www/index.html
8081 /                         -> 200 OK, ./www/site_b/index.html
8080 /images/logo.svg          -> 200 OK, ./www/images/logo.svg
8080 /images/icons/a.txt       -> 200 OK, ./www/icons/a.txt
8080 /images                   -> 200 OK, ./www/images/index.html
8080 /images/                  -> 200 OK, ./www/images/index.html
8080 /images-other             -> 404 Not Found
8080 /%2e%2e/%2e%2e/etc/passwd -> 403 Forbidden
```

기존 config 호환 확인:

```bash
./webserv config/default.conf
curl -i http://127.0.0.1:8080/
```

결과:

```text
listen sockets: 1
8080 / -> 200 OK
```

invalid config 재확인:

```bash
./webserv config/invalid/bad_port.conf
./webserv config/invalid/missing_semicolon.conf
```

결과:

```text
bad_port.conf          -> config line 3: listen port must be in 1..65535
missing_semicolon.conf -> config line 3: missing semicolon before root
```

## 이번 단계의 한계

- 같은 host:port를 여러 server block에서 공유하는 virtual host 선택은 아직 구현하지 않았다.
- `autoindex on`일 때 directory listing 생성은 아직 구현하지 않았다.
- custom error page 파일 로딩은 아직 없다.
- `client_max_body_size`는 Router의 effective config에는 포함되지만, body read 제한에는 아직 연결하지 않았다.
- POST/DELETE 실제 handler는 아직 없어서 GET 정적 파일 응답 중심으로 동작한다.

## 완료 체크

- [x] 여러 listen socket이 같은 poll loop에서 동작한다.
- [x] 서로 다른 port가 서로 다른 콘텐츠를 제공한다.
- [x] longest-prefix location을 선택한다.
- [x] server 값과 location override를 합친다.
- [x] Router가 안전한 실제 path를 제공한다.
- [x] traversal로 root 밖에 접근할 수 없다.

다음 단계는 `STEP10.md`의 method 제한과 HTTP method 처리 확장이다.
