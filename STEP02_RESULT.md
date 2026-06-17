# STEP 02 구현 결과

## 목표

`STEP02.md`의 목표는 `127.0.0.1:8080`에서 TCP 연결을 하나 받고, 고정된 HTTP 응답을 보낸 뒤 fd를 정리하는 최소 서버를 만드는 것이다.

이번 단계는 학습용 blocking 구현이다. 다음 단계에서 반드시 non-blocking fd와 `poll()` event loop로 교체한다.

## 생성/수정한 파일

| 경로 | 구현 내용 |
|---|---|
| `include/webserv/Server.hpp` | 최소 TCP 서버 클래스 선언 |
| `src/core/Server.cpp` | `socket`, `setsockopt`, `bind`, `listen`, `accept`, `send`, `close` 구현 |
| `src/main.cpp` | bootstrap 출력 대신 `Server("127.0.0.1", 8080).runOnce()` 실행 |
| `Makefile` | `src/core/Server.cpp`를 빌드 대상에 추가 |

## Server 클래스 구현 내용

`Server`는 현재 listen fd의 소유자이다.

```text
Server 생성자
-> socket 생성
-> SO_REUSEADDR 설정
-> 127.0.0.1:8080 bind
-> listen 시작

runOnce()
-> accept로 client fd 하나 받기
-> 고정 HTTP response 전송
-> client fd 닫기

Server 소멸자
-> listen fd 닫기
```

실패 경로에서는 이미 열린 fd를 닫고 `std::runtime_error`를 던진다. `main()`의 top-level `catch`가 이를 받아 에러 메시지를 출력하고 `1`로 종료한다.

## 고정 HTTP 응답

`sendFixedResponse()`가 보내는 응답은 다음과 같다.

```http
HTTP/1.1 200 OK
Content-Type: text/plain
Content-Length: 12
Connection: close

Hello World!
```

실제 전송 문자열은 HTTP 규칙에 맞게 줄바꿈을 `\r\n`으로 사용한다.

```text
Content-Length: 12
body: Hello World!
```

`Hello World!`는 12 byte이므로 `Content-Length`와 일치한다.

## main.cpp 변경 내용

이전 Step01에서는 `./webserv` 실행 시 config 경로만 출력하고 종료했다.

이번 Step02에서는 config 경로를 선택하는 코드는 유지하되, 아직 config parser가 없으므로 실제 서버 설정에는 사용하지 않는다.

현재 실행 흐름은 다음과 같다.

```text
인자 개수 검사
-> config 경로 선택
-> STEP02 안내 출력
-> 127.0.0.1:8080 서버 생성
-> client 하나 accept
-> Hello World! response 전송
-> 종료
```

## 이번 단계의 한계

- client 요청 내용을 아직 읽지 않는다.
- client 하나만 처리하고 종료한다.
- blocking `accept()`와 blocking `send()`를 사용한다.
- config 파일의 `listen`, `root`, `location` 설정을 아직 반영하지 않는다.
- `poll()` readiness 확인 없이 I/O를 수행한다.

이 한계는 `STEP03.md`에서 `poll()` 기반 event loop를 도입하면서 해결한다.

## 완료 체크

- [x] `socket(AF_INET, SOCK_STREAM, 0)`으로 TCP socket을 생성했다.
- [x] `setsockopt()`로 `SO_REUSEADDR`를 설정했다.
- [x] `127.0.0.1:8080`에 `bind()`한다.
- [x] `listen()`으로 접속 대기 상태를 만든다.
- [x] `accept()`로 client fd를 받는다.
- [x] client fd에 고정 HTTP response를 보낸다.
- [x] response 후 client fd를 닫는다.
- [x] 서버 종료 시 listen fd를 닫는다.
- [x] `curl -i http://127.0.0.1:8080/`로 응답을 확인했다.

다음 단계는 `STEP03.md`의 `poll()` 기반 이벤트 루프 구현이다.
