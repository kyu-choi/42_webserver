# STEP 06 구현 결과

## 목표

`STEP06.md`의 목표는 정상 처리와 오류 처리 결과를 공통 HTTP response 모델로 만들고, handler나 event loop가 raw response 문자열을 직접 조립하지 않도록 하는 것이다.

이번 단계에서는 `HttpResponse`, `ResponseBuilder`, `ErrorPageHandler`를 추가하고, 기존 `EventLoop`의 임시 response 문자열 생성 코드를 제거했다.

## 생성/수정한 파일

| 경로 | 구현 내용 |
|---|---|
| `include/webserv/HttpResponse.hpp` | HTTP response 모델 선언 |
| `src/http/HttpResponse.cpp` | status line/header/body 직렬화 구현 |
| `include/webserv/ResponseBuilder.hpp` | 성공, error, redirect, 405 response builder 선언 |
| `src/http/ResponseBuilder.cpp` | 공통 header와 body 정책을 적용하는 builder 구현 |
| `include/webserv/ErrorPageHandler.hpp` | 기본 error page 생성 인터페이스 선언 |
| `src/handlers/ErrorPageHandler.cpp` | 기본 HTML error page 생성 구현 |
| `include/webserv/Http.hpp` | 301, 302, 414, 500 status enum 추가 |
| `src/http/Http.cpp` | 새 status reason phrase와 mandatory status 판별 추가 |
| `src/core/EventLoop.cpp` | raw response 조립 제거, `ResponseBuilder` 사용 |
| `src/main.cpp` | 실행 메시지를 STEP06로 갱신 |
| `Makefile` | 새 response/error page 소스 빌드 대상 추가 |

## HttpResponse 구현 내용

`HttpResponse`는 response 하나의 데이터를 저장한다.

```text
version
-> HTTP/1.1

statusCode
-> 200, 400, 404 등

reasonPhrase
-> OK, Bad Request 등

headers
-> Content-Type, Location, Allow 등

body
-> binary-safe std::string body

connectionClose
-> 현재는 Connection: close 기본 정책
```

`serialize()`는 다음 형식으로 raw HTTP response를 만든다.

```text
HTTP/1.1 200 OK\r\n
Header: Value\r\n
Content-Length: N\r\n
Connection: close\r\n
\r\n
Body
```

`Content-Length`가 직접 설정되지 않은 경우 body byte 수를 기준으로 자동 생성한다. `Connection`도 직접 설정되지 않았으면 `close`를 자동 생성한다.

## body 없는 status 처리

`204 No Content`는 body가 있으면 안 되므로 `HttpResponse::serialize()`에서 body를 제거하고 `Content-Length: 0`으로 만든다.

현재 body 제거 공통 규칙은 `204`에 적용했다. 이후 HEAD 요청이나 304를 지원하면 같은 위치에 확장할 수 있다.

## ResponseBuilder 구현 내용

`ResponseBuilder`는 공통 response 생성기이다.

```text
text(status, body, contentType)
-> 일반 text/html, text/plain 등의 성공 response

error(status)
-> 기본 HTML error page response

redirect(status, location)
-> Location header가 있는 redirect response

methodNotAllowed(methods)
-> 405 response와 Allow header 생성
```

알 수 없는 status code가 들어오면 `500 Internal Server Error`로 정규화한다.

## ErrorPageHandler 구현 내용

`ErrorPageHandler::defaultErrorPage()`는 4xx/5xx error에 사용할 단순 HTML body를 만든다.

예:

```html
<!doctype html>
<html>
<head><title>404 Not Found</title></head>
<body><h1>404 Not Found</h1></body>
</html>
```

아직 custom error page는 없고, 기본 error page만 항상 생성 가능하게 유지했다.

## EventLoop 변경 내용

Step05에서는 `EventLoop.cpp` 안에 다음 임시 함수가 있었다.

```text
fixedHttpResponse()
statusHttpResponse()
```

Step06에서는 이 raw 문자열 조립 함수를 제거했다.

현재 성공 응답:

```cpp
ResponseBuilder::text(HTTP_STATUS_OK, "Hello World!", "text/plain")
```

현재 parser error 응답:

```cpp
ResponseBuilder::error(statusCode)
```

이렇게 만들어진 `HttpResponse`를 `serialize()`한 결과만 client output buffer에 넣는다. 따라서 error response도 기존 partial send 흐름을 그대로 사용한다.

## 지원 status code

공통 reason phrase에 다음 status를 지원하도록 확장했다.

```text
200 OK
201 Created
204 No Content
301 Moved Permanently
302 Found
400 Bad Request
403 Forbidden
404 Not Found
405 Method Not Allowed
413 Payload Too Large
414 URI Too Long
500 Internal Server Error
501 Not Implemented
502 Bad Gateway
504 Gateway Timeout
505 HTTP Version Not Supported
```

## 이번 단계의 한계

- custom error page 파일 로딩은 아직 없다.
- 실제 route/handler별 response 생성은 다음 단계 이후에 붙는다.
- 성공 response는 여전히 `Hello World!` 고정이다.
- keep-alive는 아직 구현하지 않고 `Connection: close`를 사용한다.

## 완료 체크

- [x] 공통 `HttpResponse` 모델이 있다.
- [x] 공통 builder가 올바른 raw response를 만든다.
- [x] 최소 status code와 reason phrase를 지원한다.
- [x] `Content-Length`가 body byte 수와 일치한다.
- [x] `204 No Content` body 제거 규칙이 있다.
- [x] 기본 HTML error page가 있다.
- [x] parser error 후 서버가 계속 동작한다.

다음 단계는 `STEP07.md`의 정적 파일 GET 구현이다.
