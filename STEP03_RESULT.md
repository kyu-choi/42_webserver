# STEP 03 구현 결과

## 목표

`STEP03.md`의 목표는 Step02의 blocking `accept()`/`send()` 구조를 제거하고, listen socket과 client socket을 하나의 `poll()` event loop에서 관리하는 것이다.

이번 단계부터 서버는 client 하나만 처리하고 종료하지 않고, 계속 실행되면서 여러 client를 처리한다.

## 생성/수정한 파일


| 경로                              | 구현 내용                                                                 |
| ------------------------------- | --------------------------------------------------------------------- |
| `include/webserv/EventLoop.hpp` | `pollfd` 목록과 client map을 관리하는 `EventLoop` 클래스 선언                      |
| `src/core/EventLoop.cpp`        | `poll()`, non-blocking `accept`, readiness 기반 `recv`/`send`, fd 제거 구현 |
| `include/webserv/Server.hpp`    | `runOnce()`를 `run()`으로 변경하고 fixed response 전송 책임 제거                   |
| `src/core/Server.cpp`           | listen fd를 non-blocking으로 설정하고 `EventLoop` 실행으로 변경                    |
| `src/main.cpp`                  | STEP03 서버 메시지 출력 후 `server.run()` 호출                                  |
| `Makefile`                      | `src/core/EventLoop.cpp`를 빌드 대상에 추가                                   |


## EventLoop 구현 내용

`EventLoop`는 다음 상태를 가진다.

```text
_listenFd
-> 새 client 연결을 받는 listen socket

_pollFds
-> poll()이 감시하는 fd 목록

_clients
-> client fd별 request buffer, response buffer, 전송 offset 저장
```

fd 관리 함수는 다음과 같다.

```text
addFd(fd, events)
-> poll 목록에 fd 등록

updateEvents(fd, events)
-> 감시 event 변경

removeFd(fd)
-> poll 목록에서 fd 제거

isListenFd(fd)
-> listen fd인지 확인

closeClient(fd)
-> poll 목록 제거, client map 제거, fd close
```

## poll loop 흐름

`EventLoop::run()`의 기본 흐름은 다음과 같다.

```text
while true:
    poll(_pollFds)
    revents가 있는 fd 목록을 snapshot으로 복사
    listen fd + POLLIN이면 accept 처리
    client fd + POLLIN이면 recv 처리
    client fd + POLLOUT이면 send 처리
    POLLERR/POLLHUP/POLLNVAL이면 client 정리
```

event 처리 중 `_pollFds`가 변경될 수 있으므로, 먼저 ready fd와 event를 별도 vector로 복사한 뒤 처리한다. 이렇게 하면 vector erase 때문에 index가 꼬이는 문제를 줄일 수 있다.

## non-blocking 처리

listen fd는 `Server::openListenSocket()`에서 `fcntl()`로 `O_NONBLOCK`을 설정한다.

accept된 client fd도 `EventLoop::handleListenEvent()`에서 `O_NONBLOCK`으로 설정한다.

현재 socket I/O 규칙은 다음과 같다.

```text
listen fd의 POLLIN 후에만 accept()
client fd의 POLLIN 후에만 recv()
client fd의 POLLOUT 후에만 send()
```

`EINTR`은 다시 시도하고, non-blocking fd에서 `EAGAIN` 또는 `EWOULDBLOCK`이 나오면 지금 처리할 데이터가 없다고 보고 해당 handler를 빠져나온다.

## client read/write 처리

새 client는 처음에 `POLLIN`만 감시한다.

```text
client 접속
-> client fd non-blocking 설정
-> _clients에 Client 생성
-> poll 목록에 POLLIN 등록
```

client에서 요청 byte가 하나 이상 들어오면 현재 단계에서는 실제 HTTP parser 없이 고정 응답을 준비한다.

```text
recv 성공
-> requestBuffer에 임시 저장
-> fixed response 준비
-> events를 POLLOUT으로 변경
```

response가 준비된 client만 `POLLOUT`을 감시하므로, response가 없는 client 때문에 busy loop가 생기지 않는다.

전송은 partial send를 고려해 `bytesSent`를 누적한다.

```text
send 일부 성공
-> bytesSent 증가
-> 남은 response가 있으면 다음 POLLOUT에서 계속 전송

send 완료
-> client fd close
-> poll 목록과 client map에서 제거
```

## 기존 코드에서 바뀐 점

Step02에서는 `Server::runOnce()`가 직접 blocking `accept()`를 호출하고, `sendFixedResponse()`로 바로 응답을 보낸 뒤 종료했다.

Step03에서는 이 책임이 `EventLoop`로 이동했다.

```text
Step02:
Server::runOnce()
-> blocking accept
-> fixed response send
-> client 하나 처리 후 종료

Step03:
Server::run()
-> EventLoop::run()
-> poll loop에서 여러 client 처리
-> 서버는 계속 실행
```

## 이번 단계의 한계

- HTTP request parser는 아직 없다.
- request가 완성되었는지 엄밀히 검사하지 않고, 받은 데이터가 있으면 고정 response를 준비한다.
- client 상태 머신과 response builder는 아직 연결하지 않았다.
- timeout 정책은 아직 없다.
- 종료는 현재 `Ctrl-C`로 한다.

이 한계는 다음 단계들에서 request buffer, client state, parser, response builder를 붙이며 해결한다.

## 완료 체크

- [x] listen/client fd를 같은 `poll()` loop에서 관리한다.
- [x] listen fd를 non-blocking으로 설정했다.
- [x] client fd를 non-blocking으로 설정했다.
- [x] `POLLIN` 이후에만 `accept()`/`recv()`를 호출한다.
- [x] `POLLOUT` 이후에만 `send()`를 호출한다.
- [x] response가 준비된 client만 `POLLOUT`을 감시한다.
- [x] `POLLERR`, `POLLHUP`, `POLLNVAL` client를 정리한다.
- [x] 여러 `curl` 요청이 각각 응답을 받는 것을 확인했다.
- [x] 아무것도 보내지 않는 client가 다른 요청을 막지 않는 것을 확인했다.

다음 단계는 `STEP04.md`의 Client 상태와 partial I/O 구조 강화이다.