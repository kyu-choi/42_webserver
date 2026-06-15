# STEP 05 - HTTP Request Parser 구현

## 이 단계의 목표

client input buffer에 누적된 raw HTTP 요청을 분석하여 method, URI, version, headers, body를 구조화한다.

이 단계에서는 먼저 request line, headers, `Content-Length` body를 완성한다. Chunked request는 [STEP15.md](STEP15.md)에서 추가한다.

## 선행 조건

- [ ] [STEP04.md](STEP04.md)가 완료되었다.
- [ ] client별 input buffer가 존재한다.
- [ ] 요청이 여러 recv로 나누어 들어와도 buffer에 누적된다.

## 만들 파일 예시

```text
include/HttpRequest.hpp
include/RequestParser.hpp
src/http/HttpRequest.cpp
src/http/RequestParser.cpp
```

## HttpRequest에 저장할 정보

```text
method
raw target
path
query string
HTTP version
headers
body
body framing 방식
파싱 결과 또는 error status
```

## Parser 상태 예시

```text
REQUEST_LINE
HEADERS
BODY_BY_LENGTH
COMPLETE
ERROR
```

## 사용자가 해야 할 일

### 1. 요청 완성 여부와 파싱을 분리

- [ ] input buffer에서 header 끝 `\r\n\r\n`을 찾는다.
- [ ] header가 아직 끝나지 않았으면 더 읽도록 한다.
- [ ] header가 완성되기 전에 handler를 호출하지 않는다.
- [ ] header가 지나치게 크면 오류 처리한다.

중요:

```text
데이터를 조금 받음
→ 현재 parser 상태만 진행
→ 부족하면 기다림
→ 다음 POLLIN 때 이어서 진행
```

### 2. request line 파싱

예:

```http
GET /images/logo.png?size=small HTTP/1.1
```

추출:

```text
method: GET
target: /images/logo.png?size=small
path: /images/logo.png
query: size=small
version: HTTP/1.1
```

- [ ] request line이 정확히 3개 요소인지 검사한다.
- [ ] method token 문법을 검사한다.
- [ ] target이 비어 있지 않은지 검사한다.
- [ ] URI 최대 길이를 검사한다.
- [ ] HTTP version을 검사한다.
- [ ] 지원하지 않는 version은 `505`로 처리한다.

### 3. header 파싱

- [ ] 각 header line을 CRLF 기준으로 분리한다.
- [ ] 첫 colon을 기준으로 name과 value를 나눈다.
- [ ] header name과 value의 공백을 정리한다.
- [ ] header name을 case-insensitive하게 조회할 수 있게 한다.
- [ ] colon 없는 header를 `400`으로 처리한다.
- [ ] 잘못된 header name을 거절한다.
- [ ] 중복 header 처리 정책을 정한다.

중요 header:

```text
Host
Content-Length
Transfer-Encoding
Content-Type
Connection
```

### 4. Host 검사

- [ ] HTTP/1.1에서 `Host`가 없으면 `400 Bad Request`로 처리한다.
- [ ] Host 값이 비어 있는 경우의 정책을 정한다.
- [ ] 현재 Mandatory 범위에서는 virtual host 선택에 사용하지 않아도 된다.

### 5. Content-Length 파싱

- [ ] 숫자가 아닌 값은 `400`으로 처리한다.
- [ ] 음수 또는 overflow를 거절한다.
- [ ] 중복 Content-Length 값이 충돌하면 거절한다.
- [ ] expected body size를 저장한다.
- [ ] header 뒤 body byte가 expected size만큼 올 때까지 기다린다.
- [ ] body가 완성되면 정확한 범위만 request body로 이동한다.

### 6. 남은 input 보존

한 recv에 다음 요청 일부가 같이 들어올 수 있다.

```text
요청 1 전체 + 요청 2 일부
```

- [ ] 현재 요청에 사용한 byte 수를 계산한다.
- [ ] 사용하지 않은 bytes를 input buffer에 남긴다.
- [ ] keep-alive를 나중에 구현할 수 있도록 구조를 만든다.

### 7. error 결과 전달

- [ ] parser가 직접 socket에 응답하지 않게 한다.
- [ ] error 종류 또는 status code를 호출자에게 전달한다.
- [ ] 호출자는 공통 ResponseBuilder로 에러 응답을 만든다.

## 반드시 테스트할 요청

정상 GET:

```bash
printf 'GET /hello?name=kyu HTTP/1.1\r\nHost: localhost\r\n\r\n' \
  | nc 127.0.0.1 8080
```

정상 POST:

```bash
printf 'POST /echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\n\r\nhello' \
  | nc 127.0.0.1 8080
```

오류:

```text
BROKEN
GET / HTTP/9.9
HTTP/1.1에서 Host 없음
colon 없는 header
Content-Length: hello
서로 다른 중복 Content-Length
header 끝이 오지 않음
body가 여러 조각으로 도착
```

## 자주 발생하는 문제

### body 일부를 다음 요청으로 착각

원인:

- header 끝 위치와 body 시작 위치 계산 오류.
- Content-Length byte 수를 정확히 사용하지 않음.

### header key를 찾지 못함

원인:

- HTTP header name이 case-insensitive라는 점을 반영하지 않음.

### body의 null byte 이후가 사라짐

원인:

- C 문자열 함수 사용.

해결:

- 수신 byte 수와 `std::string::size()` 기준으로 처리한다.

## 완료 조건

- [ ] request line을 정확히 파싱한다.
- [ ] path와 query를 분리한다.
- [ ] header를 안전하게 파싱한다.
- [ ] HTTP/1.1 Host를 검사한다.
- [ ] Content-Length body 완성을 판단한다.
- [ ] malformed request를 적절한 error로 전달한다.
- [ ] 요청 후 남은 input을 보존할 수 있다.

다음 단계: [STEP06.md](STEP06.md)
