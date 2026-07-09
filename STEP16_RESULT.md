# STEP 16 Result - CGI

## 구현 요약

이번 단계에서는 `cgi .py /usr/bin/python3;`처럼 config에 등록된 확장자를 CGI script로 실행하도록 구현했다.

CGI는 일반 handler처럼 한 번에 blocking 실행하지 않고, parent/child pipe를 `poll()` event loop에 등록해서 처리한다.

- parent -> child stdin pipe: request body 전달
- child stdout -> parent pipe: CGI output 수신
- child process: `fork()` 후 `execve()`로 interpreter 실행
- timeout: 3초 초과 시 child를 `SIGKILL`하고 `504 Gateway Timeout`
- 잘못된 CGI output 또는 비정상 종료: `502 Bad Gateway`

## 추가 및 수정된 파일

| 파일 | 내용 |
| --- | --- |
| `include/webserv/CgiHandler.hpp` | CGI 실행 결과 구조체와 `CgiHandler` 인터페이스 추가 |
| `src/handlers/CgiHandler.cpp` | CGI 대상 판별, 환경변수 생성, pipe/fork/execve, CGI output 파싱 구현 |
| `include/webserv/EventLoop.hpp` | `CgiJob` 상태와 CGI fd/job 관리 함수 선언 추가 |
| `src/core/EventLoop.cpp` | CGI pipe fd를 poll loop에 등록하고 stdin/stdout/timeout/cleanup 처리 |
| `include/webserv/HttpRequest.hpp` | CGI 환경변수 생성을 위해 전체 header accessor 추가 |
| `src/http/HttpRequest.cpp` | `headers()` accessor 구현 |
| `src/main.cpp` | `SIGPIPE` 무시, 실행 로그를 STEP16으로 갱신 |
| `Makefile` | `src/handlers/CgiHandler.cpp` 빌드 대상 추가 |
| `config/step16.conf` | CGI 테스트용 설정 추가 |
| `www/cgi-bin/hello.py` | 정상 CGI GET/POST/body/env 테스트 script |
| `www/cgi-bin/timeout.py` | timeout 테스트 script |
| `www/cgi-bin/bad_output.py` | malformed CGI output -> 502 테스트 script |

## CgiHandler 구현

### 1. CGI 대상 판별

`CgiHandler::isCgiRequest()`는 route의 filesystem path 확장자를 확인한다.

예:

```text
./www/cgi-bin/hello.py -> .py
```

그리고 route의 effective config에 다음 mapping이 있으면 CGI 대상으로 본다.

```conf
cgi .py /usr/bin/python3;
```

### 2. script file 검증

`CgiHandler::start()`는 script path를 `stat()`으로 검사한다.

- 파일이 없으면 `404 Not Found`
- regular file이 아니면 `403 Forbidden`
- pipe/fork 실패는 `500 Internal Server Error`

script path는 Router와 PathPolicy를 지난 `route.filesystemPath`를 사용하므로 `..` traversal이 제거된 안전한 경로를 사용한다.

### 3. CGI 환경변수 생성

`buildEnvironment()`에서 CGI child에게 넘길 환경변수를 만든다.

전달하는 주요 값:

```text
GATEWAY_INTERFACE=CGI/1.1
SERVER_SOFTWARE=webserv
REQUEST_METHOD
SCRIPT_NAME
SCRIPT_FILENAME
QUERY_STRING
CONTENT_LENGTH
CONTENT_TYPE
SERVER_PROTOCOL
PATH_INFO
PATH_TRANSLATED
DOCUMENT_ROOT
REDIRECT_STATUS=200
HTTP_* request headers
```

`CONTENT_LENGTH`는 `request.body().size()`를 사용한다. 그래서 `Content-Length` body뿐 아니라 STEP15의 chunked body도 decoded 길이가 전달된다.

### 4. pipe/fork/execve

CGI 실행에는 pipe 2개를 만든다.

```text
stdinPipe: parent write -> child stdin
stdoutPipe: child stdout -> parent read
```

child에서는:

```cpp
dup2(stdinPipe[0], STDIN_FILENO);
dup2(stdoutPipe[1], STDOUT_FILENO);
chdir(scriptDirectory);
execve(interpreter, argv, envp);
```

`chdir()` 후에는 script 파일명을 basename으로 넘긴다. 예를 들어 `./www/cgi-bin/hello.py`의 directory로 이동한 뒤 Python에는 `hello.py`를 넘긴다. 이 부분을 하지 않으면 상대 경로가 두 번 붙어서 script를 찾지 못한다.

child는 fd 3번 이상을 모두 닫아서 listen socket, client socket, 다른 CGI pipe fd를 상속한 채 오래 들고 있지 않게 했다.

parent에서는 사용하지 않는 pipe end를 닫고, parent 쪽 pipe fd를 non-blocking으로 만든다.

## EventLoop 변경

### 1. CgiJob 상태 추가

`CgiJob`에는 다음 정보를 저장한다.

```text
child pid
client fd
stdin pipe write fd
stdout pipe read fd
request body
stdin write offset
CGI output buffer
start time
stdin/stdout close 여부
child waitpid 회수 여부
error page 설정
```

### 2. poll fd 종류가 3개가 됨

이제 EventLoop는 ready fd를 다음 순서로 구분한다.

```text
listen fd -> accept
CGI pipe fd -> CGI stdin/stdout 처리
client fd -> client read/write 처리
```

CGI pipe fd도 `_pollFds`에 들어가므로 별도 blocking read/write를 하지 않는다.

### 3. CGI stdin 전달

`handleCgiWrite()`는 `POLLOUT` event가 온 뒤에만 request body를 pipe에 쓴다.

- partial write를 `stdinOffset`으로 추적한다.
- body를 모두 쓰면 stdin pipe를 닫는다.
- 빈 body는 CGI 시작 직후 stdin pipe를 닫아서 child에게 EOF를 전달한다.

이 EOF가 없으면 Python script가 `sys.stdin.read()`에서 계속 기다릴 수 있다.

### 4. CGI stdout 읽기

`handleCgiRead()`는 `POLLIN` 또는 `POLLHUP` event에서 stdout pipe를 읽는다.

- partial read를 `job.output`에 누적한다.
- read가 `0`이면 stdout EOF로 보고 pipe를 닫는다.
- CGI output buffer는 최대 10MB로 제한했다.

### 5. child 종료와 timeout

`checkCgiJobs()`는 poll loop마다 호출된다.

```cpp
waitpid(pid, &status, WNOHANG)
```

을 사용하므로 CGI 종료 확인 때문에 서버 전체가 멈추지 않는다.

timeout 정책:

```text
kCgiTimeoutSeconds = 3
```

3초가 지나도 child가 종료되지 않으면:

```text
kill(pid, SIGKILL)
504 Gateway Timeout
```

으로 처리한다.

### 6. cleanup

성공, 실패, timeout, client disconnect 경로에서 다음을 정리한다.

- stdin pipe fd poll 제거 및 close
- stdout pipe fd poll 제거 및 close
- CGI fd -> client fd mapping 제거
- child process 회수 또는 orphan list에 저장 후 `waitpid(..., WNOHANG)` 재시도

## CGI output 파싱

`CgiHandler::buildResponse()`는 CGI stdout을 다음 형태로 파싱한다.

```text
Content-Type: text/plain
Status: 200 OK

body
```

지원하는 내용:

- `\r\n\r\n` 또는 `\n\n` header/body separator
- `Status: 200 OK` header
- `Content-Type` header
- `Location` header
- CGI가 준 `Content-Length`는 무시하고 서버가 body 길이로 다시 계산
- `Connection`, `Transfer-Encoding`도 CGI output에서 무시

header/body separator가 없거나 CGI header 형식이 잘못되면 `502 Bad Gateway`로 처리한다.

## 테스트용 설정

`config/step16.conf`:

```conf
location /cgi-bin {
    methods GET POST;
    root ./www/cgi-bin;
    cgi .py /usr/bin/python3;
    client_max_body_size 1M;
}
```

## 실행한 검증

빌드:

```sh
make
```

결과:

```text
빌드 성공
```

서버 실행:

```sh
./webserv config/step16.conf
```

### 정상 CGI GET

```sh
curl -i 'http://127.0.0.1:8080/cgi-bin/hello.py?name=test'
```

결과:

```text
HTTP/1.1 200 OK
method=GET
query=name=test
content_length=0
script_name=/cgi-bin/hello.py
```

### 정상 CGI POST

```sh
curl -i -X POST \
  -H 'Content-Type: application/x-www-form-urlencoded' \
  -d 'name=test' \
  'http://127.0.0.1:8080/cgi-bin/hello.py'
```

결과:

```text
HTTP/1.1 200 OK
method=POST
content_length=9
content_type=application/x-www-form-urlencoded
body=name=test
```

### chunked CGI POST

raw chunked request로 확인했다.

결과:

```text
HTTP/1.1 200 OK
content_length=11
content_type=text/plain
body=hello world
```

즉 CGI stdin에는 `5\r\nhello...` 같은 raw chunk framing이 아니라 decoded body `hello world`가 들어갔다.

### timeout CGI

```sh
curl -i 'http://127.0.0.1:8080/cgi-bin/timeout.py'
```

결과:

```text
HTTP/1.1 504 Gateway Timeout
```

### non-blocking 확인

`timeout.py` 요청이 진행 중인 상태에서 다른 터미널 요청으로 `/` 정적 GET을 보냈다.

결과:

```text
HTTP/1.1 200 OK
./www/index.html 내용 즉시 응답
```

즉 CGI가 sleep 중이어도 서버 전체 poll loop는 막히지 않는다.

### malformed CGI output

```sh
curl -i 'http://127.0.0.1:8080/cgi-bin/bad_output.py'
```

결과:

```text
HTTP/1.1 502 Bad Gateway
```

### 없는 CGI script

```sh
curl -i 'http://127.0.0.1:8080/cgi-bin/missing.py'
```

결과:

```text
HTTP/1.1 404 Not Found
```

### zombie 확인

```sh
ps -o pid,ppid,stat,cmd -C python3
```

결과:

```text
남은 python3 프로세스 없음
```

## 현재 정책과 한계

- CGI timeout은 현재 3초로 고정했다.
- `PATH_INFO`는 빈 문자열 정책으로 시작했다.
- CGI output은 `Content-Type` 또는 `Location` 중 하나가 있어야 정상으로 인정한다.
- CGI가 보낸 `Content-Length`는 신뢰하지 않고 서버가 response body 길이로 다시 계산한다.
- stderr는 별도로 capture하지 않는다.

## 완료 기준 체크

- [x] 확장자와 config로 CGI 대상을 선택한다.
- [x] request method, query, decoded body, 주요 header를 CGI 환경변수/stdin으로 전달한다.
- [x] CGI pipe를 non-blocking으로 같은 poll loop에서 처리한다.
- [x] stdin EOF와 stdout EOF를 처리한다.
- [x] CGI output을 HTTP response로 만든다.
- [x] timeout은 `504`로 처리한다.
- [x] 비정상 실행/잘못된 output은 `502`로 처리한다.
- [x] child와 pipe를 성공/실패/timeout/disconnect 경로에서 정리한다.
- [x] CGI 실행 중에도 다른 요청을 처리한다.

다음 단계는 `STEP17.md`다.
