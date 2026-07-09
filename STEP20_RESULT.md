# STEP 20 Result - Bonus Cookie/Session and Multiple CGI Types

## 구현 요약

이번 단계에서는 Mandatory 기능을 유지한 상태에서 Bonus 기능을 추가했다.

추가한 Bonus:

- Cookie parsing
- Session management
- Session demo route
- Session TTL
- Session 최대 개수 제한
- 여러 CGI type 시연
- Bonus 자동 테스트

## 수정/추가된 파일

| 파일 | 내용 |
| --- | --- |
| `include/webserv/EventLoop.hpp` | session 저장 구조와 helper method 선언 추가 |
| `src/core/EventLoop.cpp` | `/session` 처리, cookie parsing, session TTL/limit 구현 |
| `config/step20.conf` | bonus 평가/시연용 config 추가 |
| `www/cgi-bin/hello.sh` | `.sh` CGI demo 추가 |
| `tests/bonus.py` | cookie/session/multiple CGI type 자동 테스트 추가 |
| `tests/integration.sh` | 전체 통합 테스트에 bonus suite 연결 |
| `tests/config_suite.py` | 정상 config 목록에 `step20.conf` 추가 |
| `TEST.md` | 한글 체크리스트의 Bonus 상태 갱신 |
| `STEP20_RESULT.md` | 이번 단계 구현 결과 문서 |

## Cookie parsing

`Cookie` header에서 `WSID` 값을 찾도록 구현했다.

예:

```http
Cookie: WSID=0123456789abcdef0123456789abcdef
```

정책:

- session id는 32자리 lowercase hex 문자열만 유효하다.
- 잘못된 cookie 값은 무시하고 새 session을 만든다.
- 일반 route에서는 cookie가 있어도 session을 만들지 않는다.

## Session management

session은 `/session` route에서만 생성된다.

저장하는 값:

- 방문 횟수
- 생성 시간
- 마지막 접근 시간

응답 예:

```text
session_id: ...
new: yes
visits: 1
created_at: ...
last_access: ...
ttl_seconds: 3
active_sessions: 1
```

응답에는 `Set-Cookie` header가 포함된다.

```http
Set-Cookie: WSID=<id>; Path=/session; Max-Age=3; HttpOnly; SameSite=Lax
```

## Session TTL과 최대 개수 제한

장시간 stress에서 session map이 무한히 커지지 않게 두 가지 제한을 넣었다.

- TTL: 3초
- 최대 session 수: 64개

새 session을 만들기 전에 만료된 session을 제거하고, 그래도 최대 개수에 도달하면 가장 오래 접근하지 않은 session을 제거한다.

## Session id 생성

session id는 `/dev/urandom`에서 16 byte를 읽어 32자리 hex 문자열로 만든다.

`/dev/urandom`을 읽지 못하는 환경에서는 `time`, `pid`, counter를 섞은 fallback 값을 사용한다.

## 여러 CGI type

기존 CGI 구조는 이미 extension별 interpreter map을 사용하고 있었다.

이번 단계에서는 이를 실제 bonus demo로 연결했다.

`config/step20.conf`:

```conf
location /cgi-bin {
    methods GET POST;
    root ./www/cgi-bin;
    cgi .py /usr/bin/python3;
    cgi .sh /bin/sh;
    client_max_body_size 1M;
}
```

`.py`와 `.sh` 모두 같은 `CgiHandler` 흐름을 사용한다.

```text
extension 확인
-> interpreter 선택
-> pipe 생성
-> fork
-> child에서 execve(interpreter, script)
-> parent에서 non-blocking pipe 처리
```

CGI type별로 별도 로직을 복사하지 않았다.

## Bonus 테스트

`tests/bonus.py`를 추가했다.

검증 항목:

- 정적 route는 session을 생성하지 않는다.
- 첫 `/session` 요청은 새 session을 만든다.
- 같은 `WSID` cookie로 다시 요청하면 방문 횟수가 증가한다.
- 잘못된 cookie는 안전하게 무시된다.
- TTL 이후 같은 cookie 요청은 새 session을 만든다.
- session 수가 64개를 넘지 않는다.
- Python CGI와 shell CGI가 모두 동작한다.
- shell CGI의 GET/POST body 전달이 동작한다.

## 실행 방법

Bonus config 실행:

```sh
./webserv config/step20.conf
```

Session demo:

```sh
curl -i -c /tmp/webserv-cookie http://127.0.0.1:8080/session
curl -i -b /tmp/webserv-cookie http://127.0.0.1:8080/session
```

Multiple CGI type demo:

```sh
curl -i "http://127.0.0.1:8080/cgi-bin/hello.py?name=python"
curl -i "http://127.0.0.1:8080/cgi-bin/hello.sh?name=shell"
curl -i -X POST http://127.0.0.1:8080/cgi-bin/hello.sh -d "bonus body"
```

자동 테스트:

```sh
python3 tests/bonus.py
```

전체 회귀 테스트:

```sh
./tests/integration.sh
```
