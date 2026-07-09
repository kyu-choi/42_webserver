# STEP 18 Result - Integration and Stress Tests

## 구현 요약

이번 단계에서는 Webserv Mandatory 기능과 안정성 경로를 한 명령으로 반복 검증할 수 있는 테스트 세트를 추가했다.

진입점은 다음 파일이다.

```sh
./tests/integration.sh
```

이 스크립트는 빌드, config, static/routing, upload/delete, malformed, chunked, slow client, CGI, stress, Valgrind smoke test를 순서대로 실행한다.

## 추가된 파일

| 파일 | 역할 |
| --- | --- |
| `config/step18.conf` | STEP18 통합 테스트용 config |
| `tests/integration.sh` | 전체 테스트 실행 진입점 |
| `tests/http_utils.py` | 서버 실행/종료, raw HTTP, response parser, 공통 assertion helper |
| `tests/config_suite.py` | 정상/오류 config 검증 |
| `tests/integration_suite.py` | static, route, error, autoindex, upload, delete 검증 |
| `tests/malformed.py` | malformed request 반복 후 서버 생존 검증 |
| `tests/chunked.py` | chunked body decode와 malformed chunk 검증 |
| `tests/slow_client.py` | 느린 header/body, 중간 disconnect 검증 |
| `tests/cgi_nonblocking.py` | CGI GET/POST/chunked/timeout/non-blocking 검증 |
| `tests/stress.py` | 동시 정적/CGI/malformed 요청 stress 검증 |
| `tests/valgrind_smoke.py` | Valgrind memory/fd smoke 검증 |

기존 작업 트리에 있던 `STEP11_RESULT.md` 수정은 이번 단계에서 건드리지 않았다.

## STEP18 테스트용 config

`config/step18.conf`는 여러 기능을 한 서버에서 검증하기 위해 만들었다.

포함한 기능:

- `8080`, `8081` 두 server block
- 정적 파일 serving
- custom error page
- redirect
- method 제한
- index 우선순위
- autoindex on/off
- `/echo` POST body echo
- multipart upload
- DELETE
- Python CGI

주요 location:

```conf
location /old {
    methods GET;
    return 301 /;
}

location /echo {
    methods POST;
    client_max_body_size 11;
}

location /upload {
    methods GET POST;
    upload on;
    upload_store ./www/uploads;
    client_max_body_size 1M;
}

location /delete {
    methods DELETE;
    root ./www/uploads;
}

location /cgi-bin {
    methods GET POST;
    root ./www/cgi-bin;
    cgi .py /usr/bin/python3;
    client_max_body_size 1M;
}
```

`8081`은 `./www/site_b`를 root로 사용해서 multi-port 테스트에서 서로 다른 body를 확인한다.

## tests/integration.sh

전체 테스트 진입점이다.

검사 순서:

1. `make re`
2. 다시 `make`를 실행해서 불필요한 relink가 없는지 확인
3. 실행 파일 이름이 `webserv`인지 확인
4. `-std=c++98` flag 확인
5. config suite
6. integration suite
7. malformed suite
8. chunked suite
9. slow client suite
10. CGI non-blocking suite
11. stress suite
12. Valgrind smoke

Valgrind는 기본으로 실행한다.

빠르게 돌리고 싶으면 다음처럼 생략할 수 있다.

```sh
SKIP_VALGRIND=1 ./tests/integration.sh
```

## 공통 테스트 helper

`tests/http_utils.py`가 공통 기능을 제공한다.

주요 기능:

- `Server(config, ports=...)`
  - `./webserv config` 실행
  - 지정 port가 열릴 때까지 대기
  - test 종료 시 `SIGINT`로 shutdown
  - 필요하면 kill fallback

- `raw_request()`
  - socket으로 raw HTTP 요청 전송

- `request()`
  - method/path/header/body로 HTTP request 생성

- `Response`
  - status line, header, body 파싱

- `assert_status()`, `assert_header()`, `assert_contains()`
  - 실패 시 어떤 기능이 깨졌는지 바로 보이게 출력

- `cleanup_uploads()`
  - 테스트가 만든 `step18-*` upload 파일 정리

## Config 테스트

파일:

```text
tests/config_suite.py
```

검증:

- 존재하지 않는 config는 실패해야 한다.
- `config/invalid/*.conf`는 모두 실패해야 한다.
- `config/default.conf`, `config/step18.conf`, `config/multiple_ports.conf`는 서버 시작에 성공해야 한다.
- `8080`, `8081`이 서로 다른 body를 제공해야 한다.

## Static, Route, Error, Autoindex 테스트

파일:

```text
tests/integration_suite.py
```

검증:

- `/` 정적 HTML `200`
- `/style.css` `Content-Type: text/css`
- `/images/logo.svg` body를 원본 파일과 byte 비교
- 없는 파일 `404`
- traversal 요청 `403`
- `/old` redirect `301`, `Location: /`
- `/post-only`에 GET 요청 시 `405`, `Allow: POST`
- 미구현 method `PUT`은 `501`
- `/has-index/`는 `home.html` 우선
- `/listing/` autoindex on
- `/private-directory/` autoindex off, `403`

## Upload와 DELETE 테스트

파일:

```text
tests/integration_suite.py
```

검증:

- multipart text upload 생성 `201`
- 같은 filename 재업로드 시 overwrite `200`
- 업로드 파일을 다시 GET해서 body 비교
- multipart binary upload 후 byte 비교
- 악성 filename `../evil.txt`는 `400`
- DELETE 성공 `204`
- 같은 파일 두 번째 DELETE `404`
- directory DELETE 거절 `403`
- traversal DELETE 거절 `403`

테스트 파일은 `step18-*` prefix를 사용하고, 테스트 전후에 정리한다.

## Malformed 테스트

파일:

```text
tests/malformed.py
```

검증:

- 잘못된 request line `400`
- Host header 누락 `400`
- header colon 누락 `400`
- 잘못된 Content-Length `400`
- malformed 반복 후 정상 GET `200`

## Chunked 테스트

파일:

```text
tests/chunked.py
```

검증:

- 여러 chunk를 decoded body로 합침
- 1 byte chunk 다수 처리
- decoded body limit 경계값 `11` bytes 통과
- decoded body limit `+1`은 `413`
- 잘못된 hex size `400`
- chunk data 뒤 CRLF 누락 `400`
- `Content-Length`와 chunked 동시 사용 `400`
- chunked CGI POST가 raw framing 없이 decoded body를 stdin으로 전달

## Slow Client 테스트

파일:

```text
tests/slow_client.py
```

검증:

- header를 한 byte씩 천천히 전송해도 정상 처리
- body를 한 byte씩 천천히 전송해도 정상 처리
- body 중간 disconnect 후 서버 생존
- response를 읽지 않는 client 후 서버 생존

## CGI Non-blocking 테스트

파일:

```text
tests/cgi_nonblocking.py
```

검증:

- CGI GET query 전달
- CGI POST body 전달
- chunked CGI POST decoded body 전달
- 잘못된 CGI output은 `502`
- 없는 CGI script는 `404`
- timeout CGI는 `504`
- timeout CGI 실행 중에도 정적 GET이 1초 안에 즉시 `200`

## Stress 테스트

파일:

```text
tests/stress.py
```

검증:

- 120개 동시 정적 GET
- 8080/8081 multi-port 동시 요청
- 80개 혼합 요청
  - 정적 GET
  - CSS GET
  - CGI GET
  - malformed request
- stress 후 정상 GET `200`

각 response는 단순 성공 개수만 보지 않고 status를 확인한다.

## Valgrind Smoke 테스트

파일:

```text
tests/valgrind_smoke.py
```

Valgrind로 서버를 실행한 뒤 다음 요청을 수행한다.

- 정적 GET
- CGI GET
- CGI timeout

검증하는 Valgrind 요약:

```text
FILE DESCRIPTORS: 3 open (3 std) at exit.
All heap blocks were freed -- no leaks are possible
ERROR SUMMARY: 0 errors
```

## 실제 실행 결과

실행:

```sh
./tests/integration.sh
```

결과 요약:

```text
[PASS] make re
[PASS] make no unnecessary relink
[PASS] binary name is webserv
[PASS] C++98 flags are present
[PASS] config suite
[PASS] integration suite
[PASS] malformed suite
[PASS] chunked suite
[PASS] slow client suite
[PASS] CGI non-blocking suite
[PASS] stress suite
[PASS] valgrind smoke
[SUMMARY] all integration checks passed
```

추가 확인:

```sh
ps -o pid,ppid,stat,cmd -C webserv -C python3
```

결과:

```text
남은 webserv/python3 프로세스 없음
```

`www/uploads` 아래에 `step18-*` 테스트 파일도 남지 않았다.

## 완료 기준 체크

- [x] 한 명령으로 주요 Mandatory 기능을 검사한다.
- [x] 테스트 실패 원인을 suite/case 이름으로 출력한다.
- [x] malformed 테스트가 있다.
- [x] slow/disconnect 테스트가 있다.
- [x] chunked 테스트가 있다.
- [x] upload/delete 테스트가 있다.
- [x] CGI non-blocking과 timeout 테스트가 있다.
- [x] stress test 후 서버 생존을 확인한다.
- [x] Valgrind memory/fd smoke 검사를 수행한다.
- [x] 테스트 산출물이 다음 실행에 영향을 주지 않게 정리한다.

다음 단계는 `STEP19.md`다.
