# STEP 12 - Request Body와 크기 제한 구현

## 이 단계의 목표

`Content-Length` 기반 request body를 완전히 수신한 뒤 처리하고, `client_max_body_size`를 정확하게 적용한다.

## 선행 조건

- [ ] [STEP11.md](STEP11.md)가 완료되었다.
- [ ] RequestParser가 Content-Length와 body 시작 위치를 파악한다.
- [ ] effective config에서 body 최대 크기를 얻을 수 있다.

## 핵심 원칙

```text
body가 완성되기 전에 handler를 실행하지 않는다.
제한 초과 body를 무한히 buffer에 저장하지 않는다.
header 크기 제한과 body 크기 제한을 분리한다.
```

## 사용자가 해야 할 일

### 1. body framing 결정

header parsing 후:

- [ ] body가 없는 요청인지 판단한다.
- [ ] `Content-Length` body인지 판단한다.
- [ ] `Transfer-Encoding: chunked`인지 판단한다.
- [ ] 충돌하는 framing header를 거절한다.

이 단계에서는 Content-Length body를 완성한다. Chunked는 [STEP15.md](STEP15.md)에서 구현한다.

### 2. Content-Length 사전 검사

- [ ] Content-Length를 안전하게 숫자로 변환한다.
- [ ] effective `client_max_body_size`와 비교한다.
- [ ] 제한보다 크면 body 전체를 받기 전에 `413` response를 준비한다.
- [ ] 거절 후 남은 body를 계속 받을지 연결을 닫을지 정책을 정한다.

### 3. body 완성 여부 판단

필요한 계산:

```text
header end 위치
body 시작 위치
현재 받은 body byte 수
expected body byte 수
```

- [ ] 현재 body가 부족하면 `READING_BODY` 상태를 유지한다.
- [ ] 다음 `POLLIN`에서 input buffer에 이어 붙인다.
- [ ] expected size만큼 받은 뒤에만 request를 complete 처리한다.
- [ ] body 이후 남은 bytes를 다음 요청용으로 보존한다.

### 4. 수신 중 크기 검사

- [ ] 실제 누적 body byte 수를 계속 확인한다.
- [ ] 설정 제한을 초과하면 `413`으로 처리한다.
- [ ] input buffer 자체의 상한도 둔다.
- [ ] body limit과 raw request limit을 혼동하지 않는다.

### 5. body를 request에 저장

- [ ] 정확한 body 범위를 binary-safe하게 복사한다.
- [ ] null byte가 있어도 보존한다.
- [ ] handler가 사용할 `HttpRequest::body`를 채운다.
- [ ] body 완료 후 parser 상태를 `COMPLETE`로 바꾼다.

### 6. timeout 연결

- [ ] body를 무한히 천천히 보내는 client를 고려한다.
- [ ] body 수신 중에도 마지막 활동 시간을 갱신한다.
- [ ] request timeout 발생 시 해당 client를 정리한다.

## 테스트

정상:

```bash
curl -i -X POST --data-binary 'hello' http://127.0.0.1:8080/echo
```

반드시 작성할 socket 테스트:

```text
Content-Length와 정확히 같은 body
body limit과 정확히 같은 크기
body limit보다 1 byte 큰 요청
Content-Length보다 짧은 body 후 대기
Content-Length보다 많은 bytes와 다음 요청
body를 한 byte씩 천천히 전송
binary null byte 포함 body
```

## status 기준

| 상황 | Status |
|---|---:|
| body 정상 | handler 결과 |
| Content-Length 문법 오류 | `400 Bad Request` |
| body 제한 초과 | `413 Payload Too Large` |
| request timeout | 정책에 따른 error 또는 연결 종료 |

## 자주 발생하는 문제

### Content-Length가 크면 서버 메모리가 계속 증가함

해결:

- header 단계에서 limit과 비교해 일찍 거절한다.

### body limit 이하인데 413

원인:

- header와 framing까지 포함한 raw input 크기를 body limit과 비교함.

해결:

- 실제 body byte 수에만 `client_max_body_size`를 적용한다.

### body 완료 전에 POST handler 호출

해결:

- Parser의 COMPLETE 상태에서만 handler를 실행한다.

## 완료 조건

- [ ] Content-Length body를 여러 recv에 걸쳐 수신한다.
- [ ] body 완성 전에 handler를 실행하지 않는다.
- [ ] 사전 및 수신 중 body limit 검사를 한다.
- [ ] 제한 초과 요청을 `413`으로 처리한다.
- [ ] binary body를 보존한다.
- [ ] 느린 body client가 다른 client를 막지 않는다.

다음 단계: [STEP13.md](STEP13.md)
