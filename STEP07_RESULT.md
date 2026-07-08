# STEP 07 구현 결과

## 목표

`STEP07.md`의 목표는 요청 URI에 해당하는 실제 파일을 `./www` 아래에서 읽어 HTTP response로 제공하는 것이다.

아직 config parser 전 단계이므로 root는 `./www`로 고정했다. `/` 요청은 임시로 `/index.html`에 매핑한다.

## 생성/수정한 파일

| 경로 | 구현 내용 |
|---|---|
| `include/webserv/StaticFileHandler.hpp` | 정적 파일 GET handler 선언 |
| `src/handlers/StaticFileHandler.cpp` | URI/path 변환, stat/access/open/read, file response 생성 |
| `include/webserv/MimeTypes.hpp` | MIME type 조회 함수 선언 |
| `src/utils/MimeTypes.cpp` | 확장자별 MIME type 매핑 구현 |
| `src/core/EventLoop.cpp` | 성공 GET 응답을 `StaticFileHandler("./www")`로 연결 |
| `Makefile` | static handler와 MIME source 빌드 대상 추가 |
| `src/main.cpp` | 실행 메시지를 STEP07로 갱신 |
| `www/index.html` | 테스트용 HTML |
| `www/style.css` | 테스트용 CSS |
| `www/images/logo.svg` | 테스트용 이미지 |

## StaticFileHandler 구현 내용

`StaticFileHandler`는 현재 GET 요청만 처리한다.

```text
HttpRequest rawTarget
-> PathPolicy::resolveUriPath()
-> ./www 기준 filesystem path 생성
-> stat/access/open/read
-> ResponseBuilder::text(200, body, mime)
```

`/` 요청은 다음처럼 처리한다.

```text
GET /
-> ./www/index.html
```

## 파일 상태 처리

정적 파일 처리의 status 정책은 다음과 같다.

```text
정상 regular file
-> 200 OK

파일 없음
-> 404 Not Found

directory 요청
-> 403 Forbidden

regular file이 아님
-> 403 Forbidden

읽기 권한 없음
-> 403 Forbidden

read 중 오류
-> 500 Internal Server Error
```

파일 body는 `read()`가 반환한 실제 byte 수만큼 `std::string::append()`로 누적한다. 따라서 text file뿐 아니라 binary file도 C 문자열 종료 문자에 잘리지 않는다.

## URI/path 보안 처리

경로 변환은 기존 `PathPolicy::resolveUriPath()`를 사용한다.

```text
query string 제외
percent decode
. segment 제거
.. segment 거절
중복 slash 정규화
location prefix 확인
root와 relative path 결합
```

`/../../etc/passwd`나 encoded traversal은 `PATH_POLICY_TRAVERSAL` 또는 decode error로 처리되어 root 밖 파일을 제공하지 않는다.

## MIME type 구현

`mimeTypeForPath()`는 확장자 기준으로 `Content-Type`을 정한다.

```text
.html/.htm -> text/html
.css       -> text/css
.js        -> application/javascript
.txt       -> text/plain
.jpg/.jpeg -> image/jpeg
.png       -> image/png
.gif       -> image/gif
.ico       -> image/x-icon
.svg       -> image/svg+xml
기타       -> application/octet-stream
```

## EventLoop 변경 내용

Step06까지 성공 응답은 고정 `Hello World!`였다.

Step07에서는 request parser가 완성한 request를 보고 다음처럼 처리한다.

```text
미구현 method
-> 501 Not Implemented

GET이 아닌 구현 method
-> 405 Method Not Allowed

GET
-> StaticFileHandler("./www").handleGet(request)
```

생성된 `HttpResponse`는 기존과 동일하게 client output buffer에 들어가고, partial send 흐름으로 전송된다.

## 이번 단계의 한계

- root가 아직 config 기반이 아니라 `./www`로 고정이다.
- directory index/autoindex는 아직 없다.
- custom error page는 아직 없다.
- image asset은 텍스트 기반 SVG를 사용했다.

## 완료 체크

- [x] `/`와 `/index.html`이 HTML을 반환한다.
- [x] `/style.css`가 `text/css`로 반환된다.
- [x] `/images/logo.svg`가 image MIME type으로 반환된다.
- [x] 없는 파일은 `404`이다.
- [x] directory 접근은 `403`이다.
- [x] traversal로 root 밖 파일을 읽을 수 없다.
- [x] 파일 body는 binary-safe 방식으로 읽는다.

다음 단계는 `STEP08.md`의 Config Parser 구현이다.
