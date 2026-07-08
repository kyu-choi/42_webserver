# STEP 10 구현 결과

## 목표

`STEP10.md`의 목표는 Router가 선택한 server/location 설정에 따라 실제 파일 handler 실행 전에 redirect, method 제한, body size 제한, custom error page를 적용하는 것이다.

이번 단계의 요청 처리 우선순위는 다음처럼 정리했다.

```text
1. client가 들어온 listen fd로 ServerConfig 선택
2. 구현하지 않은 method면 501
3. Router로 location/effective config 선택
4. redirect가 있으면 즉시 301/302
5. route에서 허용하지 않는 method면 405 + Allow
6. client_max_body_size 초과면 413
7. 아직 handler가 없는 구현 예정 method면 501
8. GET static handler 실행
9. handler error status에 custom/default error page 적용
```

## 생성/수정한 파일

| 경로 | 구현 내용 |
|---|---|
| `include/webserv/ErrorPageHandler.hpp` | custom error page 로딩 API 추가 |
| `src/handlers/ErrorPageHandler.cpp` | error_page 경로를 안전하게 resolve하고 파일 body를 읽는 기능 추가 |
| `include/webserv/ResponseBuilder.hpp` | custom body/content-type으로 error response를 만드는 overload 추가 |
| `src/http/ResponseBuilder.cpp` | custom error response 구현, redirect body 제거 |
| `include/webserv/EventLoop.hpp` | custom error와 405 응답용 helper 선언 추가 |
| `src/core/EventLoop.cpp` | redirect/method/body/custom error 우선순위 적용 |
| `src/main.cpp` | 실행 메시지를 STEP10에 맞게 변경 |
| `config/step10.conf` | redirect, method, custom error, body limit 검증용 config |
| `config/step10_missing_error.conf` | custom error fallback 검증용 config |
| `www/errors/404.html` | custom 404 page |
| `www/errors/413.html` | custom 413 page |

## Redirect 처리

location에 `return` 설정이 있으면 static handler를 호출하지 않고 바로 redirect response를 만든다.

```conf
location /old {
    return 301 /;
}
```

응답은 body 없이 `Location` header만 가진다.

```http
HTTP/1.1 301 Moved Permanently
Location: /
Content-Length: 0
```

## Method 제한

method 처리는 두 가지를 구분한다.

```text
서버가 모르는 method
-> 501 Not Implemented

서버가 아는 method지만 route에서 허용하지 않음
-> 405 Method Not Allowed + Allow header
```

예:

```text
DELETE /
-> DELETE는 서버가 아는 method
-> location / 은 GET만 허용
-> 405, Allow: GET

PUT /
-> PUT은 서버가 구현하지 않은 method
-> 501
```

현재 POST/DELETE 실제 handler는 아직 없으므로, route에서 허용된 뒤 실제 처리 단계까지 내려오면 `501 Not Implemented`로 응답한다. 다음 단계에서 upload/delete handler가 붙으면 이 부분이 실제 처리로 바뀐다.

## Body Size 제한

Router의 effective config에 들어 있는 `client_max_body_size`를 요청 처리 전에 확인한다.

```conf
location /submit {
    methods POST;
    client_max_body_size 4;
}
```

`Content-Length` 또는 parser가 읽은 body 크기가 제한을 넘으면 `413 Payload Too Large`를 반환한다.

## Custom Error Page

`ErrorPageHandler::loadCustomErrorPage()`를 추가했다.

동작 방식:

```text
1. status code에 맞는 error_page 설정 조회
2. error_page 값을 URI path처럼 처리
3. server root 안의 안전한 filesystem path로 resolve
4. regular file이고 읽을 수 있으면 body로 사용
5. 실패하면 기본 error page 사용
```

custom error page를 읽어도 원래 status code는 유지된다.

```text
/not-found
-> status: 404
-> body: www/errors/404.html
```

custom error page 로딩 실패는 다시 custom error 처리로 재귀하지 않는다. 실패하면 즉시 기본 HTML error page로 fallback한다.

## 테스트 config

`config/step10.conf`:

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
    }

    location /old {
        return 301 /;
    }

    location /submit {
        methods POST;
        client_max_body_size 4;
    }
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

redirect:

```bash
curl -i http://127.0.0.1:8080/old
```

결과:

```text
301 Moved Permanently
Location: /
Content-Length: 0
```

route별 method 제한:

```bash
curl -i -X DELETE http://127.0.0.1:8080/
```

결과:

```text
405 Method Not Allowed
Allow: GET
```

미구현 method:

```bash
curl -i -X PUT http://127.0.0.1:8080/
```

결과:

```text
501 Not Implemented
```

custom 404:

```bash
curl -i http://127.0.0.1:8080/not-found
```

결과:

```text
404 Not Found
body: custom 404 page
```

custom 413:

```bash
curl -i -X POST --data abcdef http://127.0.0.1:8080/submit
```

결과:

```text
413 Payload Too Large
body: custom 413 page
```

fallback:

```bash
./webserv config/step10_missing_error.conf
curl -i http://127.0.0.1:8080/not-found
```

결과:

```text
404 Not Found
body: 기본 error page
```

## 이번 단계의 한계

- POST upload handler는 아직 구현하지 않았다.
- DELETE handler는 아직 구현하지 않았다.
- custom error page path는 server root 기준으로 읽는다.
- custom error page에 대한 내부 redirect 방식은 구현하지 않았다.

## 완료 체크

- [x] redirect가 handler보다 먼저 처리된다.
- [x] redirect response에 올바른 `Location`이 있다.
- [x] redirect response는 body 없이 `Content-Length: 0`이다.
- [x] route별 method 제한이 적용된다.
- [x] `405`와 `501`을 구분한다.
- [x] `405`에 `Allow` header가 있다.
- [x] custom error page가 동작한다.
- [x] custom error page가 없으면 기본 error page로 fallback한다.

다음 단계는 `STEP11.md`의 GET/POST/DELETE handler 확장이다.
