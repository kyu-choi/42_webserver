# STEP 16 - CGI 구현

## 이 단계의 목표

설정된 확장자의 script를 CGI로 실행하고, request 정보를 전달하며, CGI 출력으로 HTTP response를 만든다.

CGI pipe도 client socket과 마찬가지로 non-blocking이어야 하며 같은 poll event loop에서 처리해야 한다.

## 선행 조건

- [ ] [STEP15.md](STEP15.md)가 완료되었다.
- [ ] Router가 CGI extension과 interpreter를 제공한다.
- [ ] request body는 Content-Length 또는 chunked에서 완성된 decoded body이다.
- [ ] poll loop에 새로운 fd 종류를 등록할 수 있다.

## 만들 파일 예시

```text
include/CgiHandler.hpp
src/handlers/CgiHandler.cpp
www/cgi-bin/hello.py
www/cgi-bin/timeout.py
```

## CGI 작업에 저장할 정보

```text
child pid
연결된 client fd
stdin pipe write fd
stdout pipe read fd
request body
stdin write offset
CGI output buffer
시작 시간
stdin close 여부
stdout EOF 여부
child 회수 여부
```

## 사용자가 해야 할 일

### 1. CGI 실행 대상 판단

- [ ] selected location에서 CGI가 설정되었는지 확인한다.
- [ ] request 파일 확장자와 interpreter mapping을 찾는다.
- [ ] script 실제 path를 안전하게 만든다.
- [ ] script가 존재하는 regular file인지 확인한다.
- [ ] 설정된 route 밖 script를 실행하지 않는다.

### 2. CGI 환경변수 생성

최소 전달:

```text
REQUEST_METHOD
SCRIPT_NAME
SCRIPT_FILENAME
QUERY_STRING
CONTENT_LENGTH
CONTENT_TYPE
SERVER_PROTOCOL
PATH_INFO 정책
HTTP_* request headers
```

- [ ] request method를 전달한다.
- [ ] path와 query string을 정확히 분리한다.
- [ ] decoded body 길이를 CONTENT_LENGTH로 전달한다.
- [ ] 일반 request header를 `HTTP_` 형식으로 변환한다.
- [ ] `execve()`용 null-terminated argv/envp 배열을 안전하게 만든다.

### 3. pipe 생성

두 pipe:

```text
parent → child stdin
child stdout → parent
```

- [ ] pipe 생성 실패를 처리한다.
- [ ] 필요한 parent-side pipe fd를 non-blocking으로 설정한다.
- [ ] fork 실패 시 모든 pipe fd를 닫는다.

### 4. child process 설정

child에서:

- [ ] stdin read end를 `dup2()`로 표준 입력에 연결한다.
- [ ] stdout write end를 `dup2()`로 표준 출력에 연결한다.
- [ ] 사용하지 않는 pipe end를 닫는다.
- [ ] 상속받은 listen fd를 닫는다.
- [ ] 상속받은 client fd와 다른 CGI fd를 닫는다.
- [ ] script가 상대 경로 파일을 사용할 수 있도록 올바른 directory로 `chdir()`한다.
- [ ] interpreter와 script를 `execve()`한다.
- [ ] execve 실패 시 child가 안전하게 종료되게 한다.

### 5. parent process 설정

parent에서:

- [ ] 사용하지 않는 pipe end를 닫는다.
- [ ] stdin write fd를 `POLLOUT` 감시 대상으로 등록한다.
- [ ] stdout read fd를 `POLLIN` 감시 대상으로 등록한다.
- [ ] CGI 작업과 client fd 관계를 저장한다.
- [ ] CGI 시작 시간을 저장한다.

### 6. request body를 CGI stdin에 전달

- [ ] `POLLOUT` event 후에만 pipe에 write한다.
- [ ] partial write와 offset을 처리한다.
- [ ] body를 모두 전달하면 stdin pipe를 닫는다.
- [ ] pipe를 닫아 EOF를 CGI에 전달한다.
- [ ] 빈 body도 stdin EOF를 전달한다.

Chunked request는 raw framing이 아니라 decoded body를 전달한다.

### 7. CGI stdout 읽기

- [ ] `POLLIN` event 후에만 pipe를 read한다.
- [ ] partial read를 CGI output buffer에 누적한다.
- [ ] stdout EOF를 감지한다.
- [ ] CGI output에 Content-Length가 없어도 EOF로 완료를 판단한다.
- [ ] output buffer 최대 크기 정책을 정한다.

### 8. CGI output 파싱

일반 CGI 출력:

```text
Content-Type: text/html\r\n
Status: 200 OK\r\n
\r\n
body
```

- [ ] CGI header와 body를 분리한다.
- [ ] `Content-Type`을 response에 반영한다.
- [ ] `Status` header를 지원한다.
- [ ] CGI가 Content-Length를 주지 않으면 서버가 계산한다.
- [ ] 잘못된 CGI output은 `502 Bad Gateway`로 처리한다.

### 9. child 종료와 timeout

- [ ] `waitpid(..., WNOHANG)`으로 child 상태를 확인한다.
- [ ] blocking waitpid를 사용하지 않는다.
- [ ] CGI timeout을 검사한다.
- [ ] timeout 시 `kill()` 후 child를 회수한다.
- [ ] timeout response로 `504`를 사용한다.
- [ ] 비정상 실행/출력 실패는 `502`로 처리한다.

### 10. disconnect와 cleanup

- [ ] client disconnect 시 연결된 CGI를 종료한다.
- [ ] 모든 pipe fd를 poll 목록에서 제거한다.
- [ ] 모든 pipe fd를 닫는다.
- [ ] child를 회수해 zombie를 막는다.
- [ ] 성공, 실패, timeout, disconnect 경로를 모두 점검한다.

## CGI 테스트 script 예시

정상:

```python
import os
print("Content-Type: text/plain")
print()
print("method=" + os.environ.get("REQUEST_METHOD", ""))
print("query=" + os.environ.get("QUERY_STRING", ""))
```

timeout:

```python
import time
time.sleep(30)
print("Content-Type: text/plain")
print()
print("late")
```

## 실행 및 검증

```bash
curl -i 'http://127.0.0.1:8080/cgi-bin/hello.py?name=test'
curl -i -X POST -d 'name=test' http://127.0.0.1:8080/cgi-bin/hello.py
curl -i http://127.0.0.1:8080/cgi-bin/timeout.py
```

non-blocking 확인:

```text
timeout CGI 요청을 시작한다.
CGI가 끝나기 전에 다른 터미널에서 정적 GET을 요청한다.
정적 GET이 즉시 응답해야 한다.
```

## 자주 발생하는 문제

### CGI가 body를 기다리며 종료되지 않음

원인:

- parent가 body 전송 후 stdin pipe를 닫지 않아 EOF가 전달되지 않음.

### 서버 전체가 CGI 동안 멈춤

원인:

- pipe blocking read/write.
- blocking waitpid.
- pipe를 poll loop 밖에서 처리.

### zombie process가 남음

원인:

- 성공/timeout/disconnect 경로 중 일부에서 waitpid 회수 누락.

### CGI가 상대 경로 파일을 찾지 못함

원인:

- 올바른 script directory에서 실행하지 않음.

## 완료 조건

- [ ] 확장자와 config로 CGI 대상을 선택한다.
- [ ] full request 정보와 query/body를 전달한다.
- [ ] CGI pipe를 non-blocking으로 같은 poll에서 처리한다.
- [ ] stdin EOF와 stdout EOF를 정확히 처리한다.
- [ ] CGI output을 올바른 HTTP response로 만든다.
- [ ] timeout과 실패를 `504`/`502`로 처리한다.
- [ ] child와 pipe를 모든 경로에서 정리한다.
- [ ] CGI 실행 중에도 다른 요청을 처리한다.

다음 단계: [STEP17.md](STEP17.md)
