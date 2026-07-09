# STEP 12 구현 결과

## 목표

`STEP12.md`의 목표는 `Content-Length` 기반 request body를 완전히 수신한 뒤 처리하고, route별 effective `client_max_body_size`를 정확하게 적용하는 것이다.

이번 단계에서는 body 처리 흐름을 다음처럼 정리했다.

```text
header 수신
-> RequestParser가 Content-Length와 body framing 결정
-> body가 부족하면 CLIENT_READING_BODY 유지
-> Content-Length가 route limit보다 크면 body 전체를 받기 전에 413
-> 수신 중 body byte가 route limit을 넘으면 413
-> expected body size만큼 받은 뒤 request.body에 binary-safe 저장
-> handler는 PARSE_COMPLETE 이후에만 실행
```

## 생성/수정한 파일

| 경로 | 구현 내용 |
|---|---|
| `include/webserv/EventLoop.hpp` | early body limit 검사와 timeout 정리 함수 선언 추가 |
| `src/core/EventLoop.cpp` | 사전 body limit 검사, 수신 중 body limit 검사, `/echo` POST 테스트 handler, client timeout sweeper 추가 |
| `src/http/RequestParser.cpp` | `Content-Length` + `Transfer-Encoding: chunked` 충돌 시 400 처리 |
| `src/main.cpp` | 실행 메시지를 STEP12에 맞게 변경 |
| `config/step12.conf` | Content-Length/body limit 검증용 config |

## RequestParser 동작

기존 parser는 이미 다음 흐름을 가지고 있었다.

```text
header end 찾기
request line 파싱
headers 파싱
Content-Length가 있으면 expected body size 계산
body가 부족하면 PARSE_INCOMPLETE
body가 충분하면 정확한 body 범위만 request.body에 저장
consumedBytes는 header + expected body까지만 설정
```

이번 단계에서는 framing 충돌을 보강했다.

```text
Content-Length + Transfer-Encoding: chunked
-> 400 Bad Request

Transfer-Encoding: chunked 단독
-> 501 Not Implemented
```

chunked body 자체는 `STEP15.md`에서 구현한다.

## Early Body Limit 검사

body를 전부 받기 전에 header만으로도 거절 가능한 경우를 처리한다.

```text
Content-Length > effective.clientMaxBodySize
-> 413 Payload Too Large
```

이 검사는 `RequestParser`가 `PARSER_BODY_BY_LENGTH` 상태로 incomplete를 반환했을 때 실행된다. 그래서 큰 body를 끝까지 buffer에 저장하지 않고 바로 error response를 준비한다.

## 수신 중 Body Limit 검사

현재 input buffer에서 header 뒤에 실제로 받은 body byte 수를 계산한다.

```text
receivedBodyBytes = input.size() - (headerEnd + 4)
```

수신 중에도 이 값이 route limit을 넘으면 `413`을 반환한다.

기존의 raw input buffer 1MB 제한은 body limit과 섞일 수 있어서, 별도 안전 상한으로 분리했다.

```text
raw input buffer safety cap: 64MB
route body limit: effective client_max_body_size
```

## Handler 실행 시점

handler는 `RequestParser::PARSE_COMPLETE` 이후에만 실행된다.

body가 부족한 경우:

```text
state = CLIENT_READING_BODY
handler 실행 안 함
다음 POLLIN에서 이어서 recv
```

짧은 body만 보내고 기다리는 socket 테스트에서 응답이 나오지 않는 것을 확인했다.

## Binary Body 보존

body는 `std::string::substr()`로 정확한 byte 범위만 복사한다.

null byte가 있어도 `std::string`에 그대로 보존되고, response serialize에서도 body length 기준으로 출력된다.

검증:

```text
request body bytes: 61 00 62 63 64
response body bytes: 61 00 62 63 64
```

## `/echo` 테스트 handler

STEP12 검증을 위해 `/echo` POST endpoint를 추가했다.

```conf
location /echo {
    methods POST;
    client_max_body_size 5;
}
```

route에서 POST가 허용되고 body limit을 통과하면 request body를 그대로 `application/octet-stream`으로 응답한다.

이 handler는 body parser 검증용이며, upload handler는 이후 단계에서 별도로 구현한다.

## Timeout 연결

`Client::touch()`는 recv/consume/output 진행 때 이미 마지막 활동 시간을 갱신한다.

이번 단계에서 EventLoop에 timeout sweeper를 추가했다.

```text
poll timeout: 1000ms
client timeout: 30s
```

오래 멈춘 client는 poll timeout마다 정리한다.

## 테스트 config

`config/step12.conf`:

```conf
server {
    listen 127.0.0.1:8080;
    root ./www;
    index index.html;
    client_max_body_size 10M;

    error_page 413 /errors/413.html;

    location / {
        methods GET;
    }

    location /echo {
        methods POST;
        client_max_body_size 5;
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

정상 body:

```bash
curl -i -X POST --data-binary hello http://127.0.0.1:8080/echo
```

결과:

```text
200 OK
Content-Length: 5
body: hello
```

limit과 정확히 같은 body:

```bash
curl -i -X POST --data-binary 12345 http://127.0.0.1:8080/echo
```

결과:

```text
200 OK
body: 12345
```

limit보다 1 byte 큰 body:

```bash
curl -i -X POST --data-binary 123456 http://127.0.0.1:8080/echo
```

결과:

```text
413 Payload Too Large
body: custom 413 page
```

body를 여러 recv로 나누어 전송:

```bash
{ printf 'POST /echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\n\r\nhe'; sleep 0.2; printf 'llo'; } | nc 127.0.0.1 8080
```

결과:

```text
200 OK
body: hello
```

Content-Length보다 많은 bytes와 다음 요청 bytes:

```bash
printf 'POST /echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\n\r\nhelloGET / HTTP/1.1\r\nHost: localhost\r\n\r\n' | nc 127.0.0.1 8080
```

결과:

```text
200 OK
body: hello
```

짧은 body 후 대기:

```bash
{ printf 'POST /echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\n\r\nhe'; sleep 1; } | nc -w 1 127.0.0.1 8080
```

결과:

```text
handler response 없음
```

binary null byte:

```bash
printf 'a\000bcd' | curl -s -X POST --data-binary @- http://127.0.0.1:8080/echo | od -An -t x1
```

결과:

```text
61 00 62 63 64
```

Content-Length 사전 413:

```bash
printf 'POST /echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: 6\r\n\r\n' | nc 127.0.0.1 8080
```

결과:

```text
413 Payload Too Large
```

잘못된 Content-Length:

```bash
printf 'POST /echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: abc\r\n\r\n' | nc 127.0.0.1 8080
```

결과:

```text
400 Bad Request
```

framing 충돌:

```bash
printf 'POST /echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\nTransfer-Encoding: chunked\r\n\r\nhello' | nc 127.0.0.1 8080
```

결과:

```text
400 Bad Request
```

## 이번 단계의 한계

- chunked body parsing은 아직 구현하지 않았다.
- keep-alive/pipelining은 아직 처리하지 않고 response 후 connection을 닫는다.
- `/echo`는 STEP12 검증용 handler이며 실제 upload/CGI handler는 이후 단계에서 구현한다.

## 완료 체크

- [x] Content-Length body를 여러 recv에 걸쳐 수신한다.
- [x] body 완성 전에 handler를 실행하지 않는다.
- [x] 사전 body limit 검사를 한다.
- [x] 수신 중 body limit 검사를 한다.
- [x] 제한 초과 요청을 `413`으로 처리한다.
- [x] binary body를 보존한다.
- [x] 느린 body client가 poll loop 자체를 막지 않는다.

다음 단계는 `STEP13.md`의 POST upload handler 구현이다.
