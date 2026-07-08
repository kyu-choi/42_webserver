# STEP 06 - HTTP Response와 기본 에러 구현

## 이 단계의 목표

모든 정상 및 오류 처리 결과를 일관된 HTTP response로 변환한다.

handler가 raw 문자열을 각각 직접 만들지 않도록 공통 response 모델과 builder를 만든다.

## 선행 조건

- [ ] [STEP05.md](STEP05.md)의 parser가 정상/error 결과를 전달한다.
- [ ] Client가 output buffer와 send offset을 관리한다.

## 만들 파일 예시

```text
include/HttpResponse.hpp
include/ResponseBuilder.hpp
include/ErrorPageHandler.hpp
src/http/HttpResponse.cpp
src/http/ResponseBuilder.cpp
src/handlers/ErrorPageHandler.cpp
```

## HttpResponse에 저장할 정보

```text
HTTP version
status code
reason phrase
headers
body
connection policy
```

## 사용자가 해야 할 일

### 1. status code 표 작성

최소 지원:

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

- [ ] status code를 reason phrase로 바꾸는 공통 함수를 만든다.
- [ ] 알려지지 않은 status 처리 정책을 정한다.

### 2. response 직렬화

response 형식:

```text
Status-Line\r\n
Header: Value\r\n
Header: Value\r\n
\r\n
Body
```

- [ ] status line을 만든다.
- [ ] 각 header 뒤에 `\r\n`을 붙인다.
- [ ] header와 body 사이에 `\r\n`을 추가한다.
- [ ] body는 binary-safe하게 붙인다.
- [ ] 완성된 raw response를 Client output buffer에 넣는다.

### 3. 공통 header 설정

- [ ] body가 있는 response에 정확한 `Content-Length`를 설정한다.
- [ ] 적절한 `Content-Type`을 설정한다.
- [ ] 초기 구현에서는 `Connection: close`를 설정한다.
- [ ] redirect에는 `Location`을 설정할 수 있게 한다.
- [ ] `405`에는 `Allow`를 설정할 수 있게 한다.

### 4. body가 없어야 하는 response 처리

- [ ] `204 No Content`에는 body를 넣지 않는다.
- [ ] body가 없는 response의 Content-Length 정책을 정한다.
- [ ] status에 따라 body를 제거하는 공통 규칙을 만든다.

### 5. 기본 error page 생성

- [ ] status code와 reason phrase를 보여주는 간단한 HTML을 만든다.
- [ ] 모든 4xx/5xx에서 사용할 수 있게 한다.
- [ ] error response 생성 중 오류가 나도 다시 무한 재귀하지 않게 한다.

예:

```html
<!doctype html>
<html>
<body><h1>404 Not Found</h1></body>
</html>
```

### 6. parser error 연결

- [ ] RequestParser의 error status를 ResponseBuilder에 전달한다.
- [ ] malformed request에 적절한 4xx/5xx response를 생성한다.
- [ ] error response도 partial send를 사용한다.
- [ ] response 전송 후 해당 client만 닫는다.

## 실행 및 검증

정상:

```bash
curl -i http://127.0.0.1:8080/
```

오류:

```bash
printf 'GET / HTTP/1.1\r\n\r\n' | nc 127.0.0.1 8080
printf 'BROKEN\r\n\r\n' | nc 127.0.0.1 8080
printf 'GET / HTTP/9.9\r\nHost: localhost\r\n\r\n' | nc 127.0.0.1 8080
```

검사:

- status line.
- header 끝의 빈 줄.
- `Content-Length`와 실제 body byte 수.
- error 후 새로운 정상 요청 처리.

## 자주 발생하는 문제

### 브라우저가 계속 로딩함

확인:

- Content-Length가 실제 body와 같은가?
- header 끝에 `\r\n\r\n`이 있는가?
- output buffer를 끝까지 partial send하는가?
- response 후 연결을 닫는가?

### 204 response에 body가 들어감

해결:

- status별 body 허용 정책을 builder에서 강제한다.

### error response를 만들다 서버가 종료됨

해결:

- error page 생성 경로를 단순하게 유지한다.
- custom error 처리 전 기본 error page를 항상 만들 수 있게 한다.

## 완료 조건

- [ ] 공통 HttpResponse 모델이 있다.
- [ ] 공통 builder가 올바른 raw response를 만든다.
- [ ] 최소 status code와 reason phrase를 지원한다.
- [ ] 모든 body 길이가 정확하다.
- [ ] 기본 HTML error page가 있다.
- [ ] parser error 후 서버가 계속 동작한다.

다음 단계: [STEP07.md](STEP07.md)