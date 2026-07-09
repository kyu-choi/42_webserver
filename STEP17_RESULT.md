# STEP 17 Result - Timeout and Cleanup Hardening

## 구현 요약

이번 단계에서는 기능 구현보다 서버의 생존성과 자원 정리를 강화했다.

주요 목표는 다음 상황이 반복되어도 fd, 메모리, CGI child process가 누적되지 않게 하는 것이다.

- malformed request
- request body 중간 disconnect
- CGI timeout
- CGI 실행 중 server 종료 signal
- 이벤트가 계속 들어오는 상황에서 timeout 검사 누락

## 추가 및 수정된 파일

| 파일 | 내용 |
| --- | --- |
| `include/webserv/Signal.hpp` | shutdown signal 상태를 공유하는 함수 선언 추가 |
| `src/core/Signal.cpp` | `SIGINT`/`SIGTERM` handler가 종료 요청 flag를 세우도록 구현 |
| `include/webserv/EventLoop.hpp` | `~EventLoop()`, `closeAllClients()` 선언 추가 |
| `src/core/EventLoop.cpp` | graceful shutdown, client/CGI cleanup, timeout 검사 주기, max client 제한 보강 |
| `src/main.cpp` | `SIGINT`, `SIGTERM`, `SIGPIPE` signal 정책 등록 |
| `Makefile` | `src/core/Signal.cpp` 빌드 대상 추가 |

기존에 수정되어 있던 `STEP11_RESULT.md`는 이번 STEP17 구현에서 건드리지 않았다.

## 자원 소유권 표

| 자원 | 생성 위치 | 소유자 | 정리 시점 |
| --- | --- | --- | --- |
| listen fd | `Server::createListenSocket()` | `Server` | `Server::~Server()` |
| client fd | `EventLoop::handleListenEvent()`의 `accept()` | `EventLoop` / `_clients` | response 완료, disconnect, timeout, shutdown |
| client poll entry | `EventLoop::addFd(clientFd, POLLIN)` | `EventLoop` | `closeClient()` |
| static file fd | `StaticFileHandler` 내부 | 각 handler | read 완료 또는 오류 |
| upload file fd | `UploadHandler` 내부 | 각 handler | write 완료 또는 오류 |
| delete 대상 path | `Router` / `DeleteHandler` | handler | 요청 처리 완료 |
| CGI stdin pipe write fd | `CgiHandler::start()` | `EventLoop::CgiJob` | body 전송 완료, 오류, timeout, disconnect, shutdown |
| CGI stdout pipe read fd | `CgiHandler::start()` | `EventLoop::CgiJob` | stdout EOF, 오류, timeout, disconnect, shutdown |
| CGI child pid | `CgiHandler::start()`의 `fork()` | `EventLoop::CgiJob` | 성공 종료, timeout kill, disconnect kill, shutdown cleanup |
| orphaned CGI pid | `cleanupCgiForClient()` | `EventLoop::_orphanedCgiPids` | `reapOrphanedCgiPids()` |
| request input buffer | `Client` | `Client` | request consume 또는 client close |
| response output buffer | `Client` | `Client` | send 완료 또는 client close |

## Signal 정책

새 파일:

```text
include/webserv/Signal.hpp
src/core/Signal.cpp
```

`Signal.cpp`에는 process-wide 종료 요청 flag가 있다.

```cpp
volatile sig_atomic_t gShutdownRequested = 0;
```

signal handler는 복잡한 cleanup을 직접 하지 않고 flag만 세운다.

```cpp
void handleShutdownSignal(int signalNumber)
{
    (void)signalNumber;
    gShutdownRequested = 1;
}
```

이 방식은 signal handler 안에서 `close`, `delete`, `std::map` 조작 같은 위험한 작업을 하지 않기 위한 정책이다.

`main.cpp`에서는 다음 signal을 등록한다.

```cpp
std::signal(SIGPIPE, SIG_IGN);
std::signal(SIGINT, webserv::handleShutdownSignal);
std::signal(SIGTERM, webserv::handleShutdownSignal);
```

- `SIGPIPE`: client가 끊긴 socket/pipe에 write할 때 서버가 죽지 않게 무시
- `SIGINT`: Ctrl-C 종료 요청
- `SIGTERM`: 외부 종료 요청

## EventLoop cleanup 보강

### 1. graceful shutdown

기존 EventLoop는 `while (true)`로 계속 돌았다.

이제는 다음처럼 종료 요청을 확인한다.

```cpp
while (!shutdownRequested())
```

Ctrl-C 또는 SIGTERM이 들어오면 poll loop가 빠져나오고:

```text
webserv event loop stopping
```

을 출력한 뒤 EventLoop 소멸자로 들어간다.

### 2. EventLoop 소멸자 추가

`~EventLoop()`를 추가했다.

정리 순서:

1. 모든 client fd 정리
2. 혹시 client map 밖에 남은 CGI job 정리
3. orphaned CGI pid 회수 시도

```cpp
EventLoop::~EventLoop()
{
    closeAllClients();
    ...
    cleanupCgiForClient(..., true);
    reapOrphanedCgiPids();
}
```

그래서 서버 종료 시점에도 client socket, CGI pipe, child process가 남지 않는다.

### 3. closeClient() 안전성 강화

`closeClient()` 시작 부분에 guard를 추가했다.

```cpp
if (_clients.find(fd) == _clients.end())
    return;
```

이미 정리된 fd에 대해 중복 cleanup을 시도하지 않는다.

정리 순서:

```text
연결된 CGI cleanup
poll 목록에서 client fd 제거
client map에서 제거
client fd close
```

### 4. closeAllClients() 추가

EventLoop shutdown에서 모든 client를 안전하게 닫기 위해 `closeAllClients()`를 추가했다.

map을 순회하면서 바로 erase하지 않고, 먼저 fd 목록을 복사한 뒤 하나씩 닫는다.

이유:

- map 순회 중 erase로 iterator가 깨지는 문제를 피한다.
- 각 client close가 CGI cleanup까지 같이 수행한다.

## timeout 검사 강화

기존에는 `poll()`이 timeout으로 0을 반환했을 때만 client timeout을 검사했다.

문제:

```text
계속 이벤트가 들어오면 poll timeout이 발생하지 않아 오래된 client timeout 검사가 늦어질 수 있다.
```

이번 단계에서 매 poll loop마다 다음을 먼저 수행하도록 바꿨다.

```cpp
closeTimedOutClients();
checkCgiJobs();
```

그래서 이벤트가 계속 들어오는 상황에서도:

- header가 완성되지 않는 client
- body가 완성되지 않는 client
- response를 받지 않는 client
- timeout CGI

검사가 밀리지 않는다.

현재 timeout 정책:

| 대상 | 제한 |
| --- | --- |
| client idle/header/body/response | 30초 |
| CGI 실행 | 3초 |

client timeout은 별도 HTTP response를 만들지 않고 즉시 close하는 정책이다. 이미 요청이 불완전하거나 peer가 느린 상황에서는 response보다 fd 회수가 더 안전하다.

CGI timeout은 client가 남아 있으면 `504 Gateway Timeout` response를 만든다.

## 동시 client 수 제한

EventLoop에 최대 client 수 정책을 추가했다.

```cpp
const std::size_t kMaxClients = 1024;
```

`accept()` 후 현재 client 수가 제한 이상이면 새 fd를 즉시 close한다.

```cpp
if (_clients.size() >= kMaxClients)
{
    closeFd(clientFd);
    continue;
}
```

아직 `503 Service Unavailable` status enum은 없으므로, 과도한 연결은 response 없이 닫는 정책으로 시작했다.

## 기존 제한 정책 점검

STEP17 기준으로 확인한 제한:

| 제한 | 위치 | 현재 값/정책 |
| --- | --- | --- |
| request target 최대 크기 | `RequestParser.cpp` | 8192 bytes |
| header 최대 크기 | `RequestParser.cpp` | 16KB |
| raw input buffer 최대 크기 | `EventLoop.cpp` | 64MB |
| decoded body 최대 크기 | route `client_max_body_size` | config 기반 |
| CGI output 최대 크기 | `EventLoop.cpp` | 10MB |
| 동시 client 수 | `EventLoop.cpp` | 1024 |
| CGI timeout | `EventLoop.cpp` | 3초 |
| client timeout | `EventLoop.cpp` | 30초 |

## 테스트 결과

빌드:

```sh
make
```

결과:

```text
make: Nothing to be done for 'all'.
```

### 정상 요청 유지 확인

```sh
curl -i http://127.0.0.1:8080/
curl -i 'http://127.0.0.1:8080/cgi-bin/hello.py?step=17'
```

결과:

```text
HTTP/1.1 200 OK
```

정적 GET과 CGI GET 모두 정상 동작했다.

### malformed 요청 후 생존

```sh
printf 'BAD\r\n\r\n' | nc -w 2 127.0.0.1 8080
```

결과:

```text
HTTP/1.1 400 Bad Request
```

traversal 요청:

```sh
printf 'GET /../../etc/passwd HTTP/1.1\r\nHost: localhost\r\n\r\n' \
  | nc -w 2 127.0.0.1 8080
```

결과:

```text
HTTP/1.1 403 Forbidden
```

이후 정상 GET:

```text
HTTP/1.1 200 OK
```

서버가 계속 살아 있었다.

### body 중간 disconnect

```sh
printf 'POST /echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: 10\r\n\r\nabc' \
  | nc -w 1 127.0.0.1 8080
```

결과:

```text
성공 response 없이 연결 종료
```

이후 정상 GET:

```text
HTTP/1.1 200 OK
```

불완전 body client가 정리된 뒤에도 서버가 계속 동작했다.

### CGI timeout

```sh
curl -i 'http://127.0.0.1:8080/cgi-bin/timeout.py'
```

결과:

```text
HTTP/1.1 504 Gateway Timeout
```

프로세스 확인:

```sh
ps -o pid,ppid,stat,cmd -C python3
```

결과:

```text
남은 python3 프로세스 없음
```

### CGI 실행 중 shutdown

`timeout.py` 요청이 실행 중인 상태에서 서버에 Ctrl-C를 보냈다.

서버 출력:

```text
webserv event loop stopping
```

프로세스 확인:

```sh
ps -o pid,ppid,stat,cmd -C webserv -C python3
```

결과:

```text
남은 webserv/python3 프로세스 없음
```

## Valgrind 결과

실행:

```sh
valgrind --leak-check=full --show-leak-kinds=all --track-fds=yes \
  ./webserv config/step16.conf
```

Valgrind 중 수행한 요청:

```text
정적 GET
CGI GET
CGI timeout
Ctrl-C shutdown
```

요약:

```text
FILE DESCRIPTORS: 3 open (3 std) at exit.
HEAP SUMMARY:
    in use at exit: 0 bytes in 0 blocks
All heap blocks were freed -- no leaks are possible
ERROR SUMMARY: 0 errors from 0 contexts
```

fd leak, memory leak, Valgrind error가 발견되지 않았다.

## 현재 정책과 남은 확장 포인트

- client timeout은 30초 고정이다.
- CGI timeout은 3초 고정이다.
- 동시 client 제한 초과 시 response 없이 close한다.
- session bonus가 생기면 TTL과 최대 개수 제한을 별도로 추가해야 한다.
- 장시간 stress test는 STEP18 이후 통합 테스트에서 더 길게 돌리는 것이 좋다.

## 완료 기준 체크

- [x] 모든 주요 fd와 child pid의 소유권을 정리했다.
- [x] client cleanup이 연결된 CGI 작업까지 정리한다.
- [x] CGI cleanup이 stdin/stdout pipe와 child pid를 정리한다.
- [x] client timeout 검사가 이벤트가 계속 들어와도 수행된다.
- [x] CGI timeout이 `504`로 처리된다.
- [x] malformed 요청 반복 후 서버가 살아 있다.
- [x] request 중간 disconnect 후 서버가 살아 있다.
- [x] timeout CGI 후 zombie가 남지 않는다.
- [x] SIGPIPE 때문에 서버가 종료되지 않게 했다.
- [x] SIGINT/SIGTERM graceful shutdown 경로를 추가했다.
- [x] Valgrind에서 memory/fd leak이 발견되지 않았다.

다음 단계는 `STEP18.md`다.
