# STEP 05 구현 결과

## 목표

`STEP05.md`의 목표는 client input buffer에 누적된 raw HTTP request를 분석해서 method, URI, version, headers, body를 구조화하는 것이다.

이번 단계에서는 request line, headers, `Content-Length` body를 구현했다. Chunked request는 `STEP15.md`에서 추가할 범위이므로 현재는 `501 Not Implemented`로 응답한다.

## 생성/수정한 파일

| 경로 | 구현 내용 |
|---|---|
| `include/webserv/HttpRequest.hpp` | 파싱된 request 데이터 구조 선언 |
| `src/http/HttpRequest.cpp` | method, target, path/query, version, headers, body 저장 구현 |
| `include/webserv/RequestParser.hpp` | parser 상태와 parse 결과 인터페이스 선언 |
| `src/http/RequestParser.cpp` | request line, headers, Host, Content-Length body 파싱 구현 |
| `include/webserv/Client.hpp` | client별 `HttpRequest`, `RequestParser`, input 소비 함수 추가 |
| `src/core/Client.cpp` | parser/request 접근자와 `consumeInput()` 구현 |
| `src/core/EventLoop.cpp` | header-end 임시 판단 제거, `RequestParser` 결과 기반 response 준비 |
| `src/main.cpp` | 실행 메시지를 STEP05로 갱신 |
| `Makefile` | `HttpRequest.cpp`, `RequestParser.cpp` 빌드 대상 추가 |

## HttpRequest 구현 내용

`HttpRequest`는 parser가 추출한 HTTP request 정보를 저장한다.

```text
methodText
-> 원본 method 문자열, 예: GET

method
-> 내부 enum, 예: HTTP_METHOD_GET

rawTarget
-> 원본 target, 예: /hello?name=kyu

path / query
-> /hello 와 name=kyu 로 분리

version
-> HTTP/1.1

headers
-> lower-case key로 저장해서 case-insensitive 조회 가능

body
-> Content-Length 기준으로 정확히 잘라낸 body

bodyFraming
-> BODY_NONE, BODY_CONTENT_LENGTH, BODY_CHUNKED

errorStatus
-> parser error가 있으면 400, 501, 505 등 저장
```

## RequestParser 구현 내용

`RequestParser::parse()`는 input buffer를 직접 수정하지 않는다. 대신 다음 결과를 반환한다.

```text
PARSE_INCOMPLETE
-> 아직 header 또는 body byte가 부족함

PARSE_COMPLETE
-> request 하나가 완성됨

PARSE_ERROR
-> malformed request. errorStatus()로 status 전달
```

사용한 byte 수는 `consumedBytes()`로 알려준다. `EventLoop`는 이 값을 이용해 `Client::consumeInput()`을 호출한다. 이렇게 하면 한 recv에 `요청 1 전체 + 요청 2 일부`가 들어오는 경우에도 남은 bytes를 보존할 수 있다.

## Request Line 파싱

예시는 다음과 같다.

```http
GET /images/logo.png?size=small HTTP/1.1
```

파싱 결과:

```text
method: GET
target: /images/logo.png?size=small
path: /images/logo.png
query: size=small
version: HTTP/1.1
```

검사하는 오류:

```text
요소가 정확히 3개가 아님 -> 400
method token 문법 오류 -> 400
target이 비어 있거나 /로 시작하지 않음 -> 400
target이 너무 김 -> 400
HTTP/1.1이 아님 -> 505
```

method token은 유효하지만 `GET`, `POST`, `DELETE`가 아니면 request 자체는 파싱 완료로 보고, `EventLoop`가 `501 Not Implemented` 응답을 만든다.

## Header 파싱

header는 CRLF 기준으로 나누고, 첫 colon을 기준으로 name/value를 분리한다.

검사하는 오류:

```text
colon 없는 header -> 400
잘못된 header name token -> 400
HTTP/1.1에서 Host 없음 -> 400
Host 값이 비어 있음 -> 400
Content-Length가 숫자가 아님 -> 400
Content-Length overflow -> 400
중복 Content-Length 값이 서로 다름 -> 400
Transfer-Encoding: chunked -> 501
```

header name은 lower-case로 저장해서 `Host`, `host`, `HOST`를 같은 key로 조회할 수 있다.

## Content-Length Body 처리

`Content-Length`가 있으면 header 뒤 body byte가 정확히 그 크기만큼 들어올 때까지 `PARSE_INCOMPLETE`를 반환한다.

body가 완성되면:

```text
request.body = input.substr(bodyStart, contentLength)
consumedBytes = header bytes + body bytes
```

body 뒤에 남은 bytes는 client input buffer에 그대로 남긴다.

## EventLoop 변경 내용

Step04에서는 input buffer에 `\r\n\r\n`만 보이면 곧바로 고정 response를 준비했다.

Step05에서는 다음 흐름으로 바뀌었다.

```text
recv 성공
-> Client input buffer에 append
-> RequestParser::parse(input, request)
-> incomplete이면 계속 POLLIN
-> complete이면 consumed bytes 제거 후 response 준비
-> error이면 parser status로 error response 준비
```

아직 ResponseBuilder 단계가 아니므로 success response는 기존 `Hello World!`를 유지한다. Error response는 간단한 text/plain 응답으로 만든다.

## 이번 단계의 한계

- response body는 아직 request 내용을 반영하지 않고 `Hello World!`로 고정이다.
- route/location 선택은 아직 없다.
- keep-alive는 아직 실제로 처리하지 않고 응답 후 close한다.
- chunked body는 아직 구현하지 않았다.
- parser error page는 아직 custom error page가 아니라 기본 text/plain이다.

## 완료 체크

- [x] request line을 정확히 파싱한다.
- [x] path와 query를 분리한다.
- [x] header를 안전하게 파싱한다.
- [x] HTTP/1.1 Host를 검사한다.
- [x] Content-Length body 완성을 판단한다.
- [x] malformed request를 status code로 전달한다.
- [x] 요청 후 남은 input을 보존할 수 있는 구조를 만들었다.
- [x] 정상 GET, 정상 POST, malformed request를 검증했다.

다음 단계는 `STEP06.md`의 HTTP Response와 기본 에러 구현이다.
