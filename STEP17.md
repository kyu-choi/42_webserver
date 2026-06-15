# STEP 17 - Timeout과 자원 정리 강화

## 이 단계의 목표

느린 client, disconnect, CGI timeout, malformed request가 반복되어도 서버가 계속 동작하고 fd, 메모리, child process가 누적되지 않게 한다.

기능이 정상 동작하는 것만으로는 Webserv Mandatory를 만족하지 않는다. 서버는 비정상 상황에서도 살아 있어야 한다.

## 선행 조건

- [ ] [STEP16.md](STEP16.md)의 CGI가 완료되었다.
- [ ] client, listen, CGI fd를 poll loop에서 관리한다.
- [ ] 모든 주요 Mandatory handler가 동작한다.

## 사용자가 해야 할 일

### 1. 자원 소유권 표 작성

각 자원에 대해 누가 만들고 누가 정리하는지 기록한다.

| 자원 | 생성 위치 | 소유자 | 정리 시점 |
|---|---|---|---|
| listen fd | Server 초기화 | Server | 서버 종료 |
| client fd | accept | Client/EventLoop | disconnect/response 완료/timeout |
| file fd | handler | handler | read/write 완료 또는 오류 |
| CGI stdin pipe | CGI 시작 | CGI 작업 | body 전송 완료/오류 |
| CGI stdout pipe | CGI 시작 | CGI 작업 | EOF/오류 |
| child pid | fork | CGI 작업 | 종료/timeout/disconnect |

- [ ] 모든 자원이 표에 포함되었는지 확인한다.
- [ ] 소유자가 둘 이상이거나 없는 자원이 없게 한다.

### 2. 공통 client cleanup 강화

client cleanup에서:

- [ ] poll 목록에서 client fd 제거.
- [ ] client map에서 Client 제거.
- [ ] 연결된 CGI 작업 확인.
- [ ] 연결된 CGI가 있으면 종료 및 정리.
- [ ] client fd close.
- [ ] 이미 정리된 client에도 안전하게 동작.

### 3. CGI cleanup 강화

CGI cleanup에서:

- [ ] stdin pipe를 poll 목록에서 제거하고 닫는다.
- [ ] stdout pipe를 poll 목록에서 제거하고 닫는다.
- [ ] 필요한 경우 child에 signal을 보낸다.
- [ ] `waitpid(..., WNOHANG)` 및 후속 회수 정책을 적용한다.
- [ ] client와 CGI 관계를 제거한다.
- [ ] 성공, exec 실패, timeout, disconnect 경로를 모두 처리한다.

### 4. client timeout 구현

필요한 timeout:

```text
header가 완성되지 않는 요청
body가 완성되지 않는 요청
idle keep-alive 연결
response를 받지 않는 느린 client
```

- [ ] Client의 마지막 활동 시간을 저장한다.
- [ ] recv/send 성공 시 활동 시간을 갱신한다.
- [ ] event loop에서 주기적으로 timeout을 검사한다.
- [ ] timeout client를 안전하게 정리한다.
- [ ] timeout response를 보낼지 즉시 닫을지 정책을 정한다.

### 5. CGI timeout 구현

- [ ] CGI 시작 시간을 저장한다.
- [ ] 제한 시간을 초과하면 `kill()`한다.
- [ ] child를 반드시 회수한다.
- [ ] client가 연결되어 있으면 `504 Gateway Timeout`을 만든다.
- [ ] client가 이미 끊겼다면 response 없이 자원만 정리한다.

### 6. buffer와 자원 제한

- [ ] request line 최대 크기를 둔다.
- [ ] header 최대 크기를 둔다.
- [ ] decoded body 최대 크기를 적용한다.
- [ ] output buffer 크기 정책을 검토한다.
- [ ] CGI output 최대 크기 또는 처리 정책을 정한다.
- [ ] 동시 client 수가 과도한 경우의 정책을 정한다.
- [ ] session Bonus가 있다면 TTL/최대 개수를 둔다.

### 7. 예외와 signal 정책

- [ ] request 하나의 오류가 event loop 밖으로 전파되지 않게 한다.
- [ ] allocation 실패 등 자원 부족 처리 경로를 검토한다.
- [ ] cleanup 함수가 예외를 던지지 않게 한다.
- [ ] SIGPIPE 때문에 서버가 종료되지 않게 한다.
- [ ] 종료 signal에서 listen/client/CGI 자원을 정리한다.

### 8. fd 상태 일관성 검사

각 fd마다 확인:

```text
열려 있는가?
non-blocking인가?
poll에 등록되어 있는가?
어떤 객체가 소유하는가?
close 후 poll/map에서도 제거되는가?
fork 후 parent/child 중 누가 닫는가?
```

## 수동 테스트

```text
연결 후 아무 요청도 보내지 않기
header 일부만 보내고 대기
body 일부만 보내고 대기
request 중간 disconnect
response 중간 disconnect
CGI 중간 disconnect
무한 실행 CGI
malformed 요청 반복
```

각 테스트 후 정상 GET을 보내 서버 생존 여부를 확인한다.

## Valgrind 점검

```bash
valgrind --leak-check=full --show-leak-kinds=all --track-fds=yes \
  ./webserv config/default.conf
```

Valgrind 실행 중:

- GET.
- upload.
- DELETE.
- CGI.
- timeout.
- disconnect.
- malformed request.

를 수행한 뒤 서버를 정상 종료하고 결과를 확인한다.

## 자주 발생하는 문제

### fd 수가 계속 증가함

확인:

- poll 목록에서만 제거하고 close하지 않았는가?
- client close 시 연결된 CGI pipe를 남겼는가?
- error path에서 file fd를 닫지 않았는가?

### zombie process가 증가함

확인:

- CGI 성공 경로만 waitpid하고 실패/timeout 경로는 누락하지 않았는가?

### timeout 검사 때문에 CPU 사용량이 높음

확인:

- poll timeout과 timeout 검사 주기가 적절한가?
- busy loop를 만들지 않는가?

## 완료 조건

- [ ] 모든 fd와 child의 소유권이 명확하다.
- [ ] client와 CGI timeout이 동작한다.
- [ ] disconnect 시 관련 자원이 모두 정리된다.
- [ ] malformed 요청 반복 후 서버가 살아 있다.
- [ ] timeout CGI 반복 후 zombie가 남지 않는다.
- [ ] Valgrind에서 memory/fd leak을 해결했다.
- [ ] 장시간 stress 후 메모리와 fd가 지속 증가하지 않는다.

다음 단계: [STEP18.md](STEP18.md)
