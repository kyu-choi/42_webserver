# STEP 03 - poll 기반 이벤트 루프 만들기

## 이 단계의 목표

listen socket과 여러 client socket을 **하나의 `poll()` event loop**에서 처리한다.

이 단계부터 blocking 임시 구조를 제거하고 Webserv의 핵심 구조를 만든다.

## 선행 조건

- [ ] [STEP02.md](STEP02.md)의 최소 TCP 서버가 동작한다.
- [ ] listen fd와 client fd의 차이를 설명할 수 있다.
- [ ] `pollfd`의 `fd`, `events`, `revents` 역할을 이해한다.

## 만들 파일 예시

```text
include/EventLoop.hpp
src/core/EventLoop.cpp
```

## 핵심 규칙

```text
listen fd의 POLLIN 후 accept
client fd의 POLLIN 후 recv
client fd의 POLLOUT 후 send
socket/pipe I/O를 readiness event 없이 실행하지 않음
```

## 사용자가 해야 할 일

### 1. fd 관리 구조 만들기

- [ ] poll이 감시할 `pollfd` 목록을 만든다.
- [ ] fd 종류를 구분할 자료구조를 만든다.
- [ ] listen fd인지 client fd인지 확인하는 방법을 정한다.
- [ ] fd 등록, event 변경, 제거 함수를 만든다.

필요한 함수 예시:

```text
addFd
updateEvents
removeFd
isListenFd
closeClient
```

### 2. listen fd non-blocking 설정

- [ ] `fcntl()`로 listen fd를 non-blocking으로 설정한다.
- [ ] listen fd를 `POLLIN` 감시 대상으로 등록한다.
- [ ] 설정 실패 시 서버 초기화를 중단하고 fd를 정리한다.

### 3. event loop 작성

기본 구조:

```text
while server is running:
    poll 호출
    각 pollfd의 revents 확인
    listen fd 처리
    client read 처리
    client write 처리
    error/disconnect 처리
```

- [ ] 하나의 `poll()` 호출 흐름을 유지한다.
- [ ] read와 write event를 동시에 감시할 수 있게 한다.
- [ ] `poll()` 실패 처리 정책을 정한다.
- [ ] event 처리 중 fd 목록이 변경될 때 iterator/index 문제를 막는다.

### 4. accept 처리

- [ ] listen fd에 `POLLIN`이 있을 때만 `accept()`한다.
- [ ] accept한 client fd를 non-blocking으로 설정한다.
- [ ] client fd를 poll 목록에 등록한다.
- [ ] client fd가 어느 listen fd에서 왔는지 저장할 준비를 한다.

### 5. client read 처리

- [ ] client fd에 `POLLIN`이 있을 때만 `recv()`한다.
- [ ] 받은 데이터가 있으면 임시 buffer 또는 client buffer에 저장한다.
- [ ] 연결 종료를 감지하면 client를 정리한다.
- [ ] 이 단계에서는 요청 내용과 상관없이 고정 response를 준비해도 된다.

### 6. client write 처리

- [ ] response가 준비된 client만 `POLLOUT`을 감시한다.
- [ ] client fd에 `POLLOUT`이 있을 때만 `send()`한다.
- [ ] 전송 완료 후 client를 닫는다.
- [ ] 항상 `POLLOUT`을 등록해 busy loop가 생기지 않게 한다.

### 7. 오류와 disconnect 처리

- [ ] `POLLERR`를 처리한다.
- [ ] `POLLHUP`를 처리한다.
- [ ] `POLLNVAL`을 처리한다.
- [ ] close 전에 poll 목록과 관련 map에서 fd를 제거한다.
- [ ] 이미 닫힌 fd를 다시 닫지 않는다.

## 실행 및 검증

동시 요청:

```bash
curl http://127.0.0.1:8080/ &
curl http://127.0.0.1:8080/ &
curl http://127.0.0.1:8080/ &
wait
```

느린 client:

```bash
nc 127.0.0.1 8080
```

`nc` 연결을 열어두고 아무것도 보내지 않은 상태에서 다른 터미널로 curl 요청을 보낸다.

disconnect:

```text
nc로 연결 후 즉시 종료한다.
그 뒤 새로운 curl 요청이 정상 처리되는지 확인한다.
```

## 확인할 동작

- [ ] 여러 client가 각각 응답을 받는다.
- [ ] 요청을 보내지 않는 client가 다른 요청을 막지 않는다.
- [ ] client disconnect 후 서버가 계속 동작한다.
- [ ] idle 상태에서 CPU 사용량이 비정상적으로 높지 않다.

## 자주 발생하는 문제

### CPU가 100% 사용됨

주요 원인:

- response가 없는 client에도 항상 `POLLOUT`을 등록함.
- 처리할 event가 없는데 poll timeout을 0으로 사용함.

### client close 후 다른 client가 이상하게 동작함

주요 원인:

- vector에서 요소 삭제 후 index가 변경됨.
- poll 목록에서는 지웠지만 client map에는 남음.
- 닫힌 fd 번호가 나중에 재사용됨.

### readiness 없이 I/O 호출

다음 구조가 남아 있지 않은지 검사한다.

```text
event loop 밖의 accept
POLLIN 없는 recv/read
POLLOUT 없는 send/write
```

## 완료 조건

- [ ] listen/client fd를 같은 poll loop에서 관리한다.
- [ ] 모든 socket I/O 전에 readiness를 확인한다.
- [ ] 여러 client를 동시에 처리한다.
- [ ] disconnect와 poll error를 처리한다.
- [ ] response가 없는 client의 `POLLOUT`을 비활성화한다.
- [ ] fd 제거가 일관되게 동작한다.

다음 단계: [STEP04.md](STEP04.md)
