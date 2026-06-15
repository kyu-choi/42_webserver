# STEP 15 - Transfer-Encoding: chunked 구현

## 이 단계의 목표

`Transfer-Encoding: chunked` request body를 파싱하고 framing을 제거한 decoded body를 만든다.

CGI와 upload handler에는 decoded body만 전달해야 한다.

## 선행 조건

- [ ] [STEP14.md](STEP14.md)가 완료되었다.
- [ ] RequestParser가 상태 머신으로 동작한다.
- [ ] Content-Length body와 body size limit이 동작한다.

## Chunked body 예시

```http
POST /echo HTTP/1.1
Host: localhost
Transfer-Encoding: chunked

5\r
hello\r
6\r
 world\r
0\r
\r
```

decoded body:

```text
hello world
```

## 추가할 Parser 상태

```text
BODY_CHUNK_SIZE
BODY_CHUNK_DATA
BODY_CHUNK_CRLF
BODY_CHUNK_TRAILERS
COMPLETE
```

## 사용자가 해야 할 일

### 1. framing 방식 결정

- [ ] `Transfer-Encoding` header를 case-insensitive하게 조회한다.
- [ ] chunked 요청인지 판단한다.
- [ ] `Content-Length`와 chunked가 동시에 있을 때 거절 정책을 적용한다.
- [ ] 지원하지 않는 Transfer-Encoding을 거절한다.

### 2. chunk size line 파싱

- [ ] 다음 CRLF까지 size line을 기다린다.
- [ ] size를 16진수 숫자로 변환한다.
- [ ] 잘못된 문자와 overflow를 거절한다.
- [ ] chunk extension 처리 정책을 정한다.
- [ ] 현재 chunk의 예상 data 크기를 저장한다.

### 3. chunk data 수신

- [ ] size만큼 data가 도착할 때까지 기다린다.
- [ ] 도착한 data를 decoded body에 append한다.
- [ ] data 뒤 정확한 CRLF를 검사한다.
- [ ] CRLF 누락은 `400`으로 처리한다.
- [ ] 다음 chunk size 상태로 이동한다.

### 4. 종료 chunk 처리

- [ ] size가 `0`이면 body data 종료로 판단한다.
- [ ] trailer header를 지원하거나 안전하게 소비하는 정책을 정한다.
- [ ] 마지막 빈 줄까지 확인한다.
- [ ] parser 상태를 COMPLETE로 바꾼다.

### 5. decoded body 크기 제한

- [ ] chunk data를 append할 때마다 decoded body 크기를 검사한다.
- [ ] `client_max_body_size`는 decoded body에 적용한다.
- [ ] raw chunk framing 크기 때문에 정상 요청을 `413`으로 처리하지 않는다.
- [ ] 제한 초과 시 추가 body를 무한히 저장하지 않는다.

### 6. handler와 CGI 연결

- [ ] upload/POST handler에 decoded body를 전달한다.
- [ ] CGI stdin에도 decoded body를 전달한다.
- [ ] raw framing bytes가 handler에 노출되지 않게 한다.

## 반드시 작성할 테스트

정상:

```text
한 개 chunk
여러 개 작은 chunk
chunk size와 body limit 경계
빈 body: 0 chunk
chunked CGI POST
```

오류:

```text
유효하지 않은 16진수 size
chunk data 길이 부족
chunk 뒤 CRLF 누락
종료 0 chunk 누락
Content-Length와 chunked 동시 사용
decoded body 제한 1 byte 초과
```

Python socket으로 raw 요청을 직접 작성하는 것이 가장 정확하다.

## 자주 발생하는 문제

### 정상 body인데 413

원인:

- raw input buffer 또는 framing 크기를 body limit과 비교함.

해결:

- decoded body 누적 크기만 검사한다.

### 다음 chunk size 일부를 data로 처리

원인:

- data 크기와 뒤 CRLF 범위 계산 오류.

### CGI가 raw chunk 문자열을 받음

해결:

- CGI 시작 전에 request body가 decoded body인지 확인한다.

## 완료 조건

- [ ] chunk size를 16진수로 파싱한다.
- [ ] 여러 chunk data를 정확히 합친다.
- [ ] 종료 chunk와 trailer 끝을 처리한다.
- [ ] malformed chunk를 `400`으로 처리한다.
- [ ] decoded body 기준으로 `413`을 판단한다.
- [ ] upload와 CGI가 decoded body를 받는다.
- [ ] malformed 요청 후 서버가 계속 동작한다.

다음 단계: [STEP16.md](STEP16.md)
