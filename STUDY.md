# Webserv 코드 공부 정리

이 문서는 현재 코드베이스를 공부하기 위한 설명서이다.

정리 순서:

1. 전체 실행 흐름
2. 헤더 파일에 정의된 클래스/구조체/함수의 역할
3. 구현 파일별 역할과 함수 설명
4. config, test, www 파일의 역할
5. 평가 중 설명하기 좋은 흐름

---

## 1. 전체 실행 흐름

Webserv의 큰 흐름은 다음과 같다.

```text
main
-> ConfigParser가 config file 파싱
-> Server가 listen socket 생성
-> EventLoop가 poll loop 시작
-> listen socket readable event
-> accept로 Client 생성
-> client socket readable event
-> RequestParser가 HTTP request 파싱
-> Router가 server/location config 매칭
-> handler 선택
   - StaticFileHandler
   - UploadHandler
   - DeleteHandler
   - CgiHandler
   - session bonus route
-> HttpResponse 생성
-> client socket writable event
-> partial send
-> cleanup
```

핵심 소유 관계:

```text
main.cpp
  owns ConfigParser temporarily
  owns Server

Server
  owns listen sockets
  creates EventLoop

EventLoop
  owns poll fd list
  owns active Client objects
  owns active CGI jobs
  owns bonus session map

Client
  owns input/output buffer
  owns HttpRequest
  owns RequestParser
```

---

## 2. 헤더 파일부터 보기

헤더 파일은 “다른 파일이 사용할 수 있는 인터페이스”를 정의한다.  
즉, `.hpp`를 보면 이 프로젝트의 설계 지도를 먼저 볼 수 있다.

### include/webserv/Http.hpp

HTTP의 가장 기본적인 공통 타입과 helper 함수가 정의되어 있다.

#### `enum HttpMethod`

```cpp
HTTP_METHOD_GET
HTTP_METHOD_POST
HTTP_METHOD_DELETE
HTTP_METHOD_NOT_IMPLEMENTED
```

요청 method를 문자열 그대로 들고 다니지 않고 enum으로 바꿔서 처리하기 위한 타입이다.

사용 위치:

- `RequestParser`가 request line의 `GET`, `POST`, `DELETE`를 파싱할 때 사용한다.
- `Router`와 `EventLoop`가 method 허용 여부를 검사할 때 사용한다.
- `CgiHandler`가 CGI 환경변수 `REQUEST_METHOD`를 만들 때 사용한다.

#### `enum HttpStatus`

HTTP 응답 status code를 코드 상수로 관리한다.

예:

- `HTTP_STATUS_OK = 200`
- `HTTP_STATUS_CREATED = 201`
- `HTTP_STATUS_NO_CONTENT = 204`
- `HTTP_STATUS_MOVED_PERMANENTLY = 301`
- `HTTP_STATUS_BAD_REQUEST = 400`
- `HTTP_STATUS_FORBIDDEN = 403`
- `HTTP_STATUS_NOT_FOUND = 404`
- `HTTP_STATUS_METHOD_NOT_ALLOWED = 405`
- `HTTP_STATUS_PAYLOAD_TOO_LARGE = 413`
- `HTTP_STATUS_URI_TOO_LONG = 414`
- `HTTP_STATUS_INTERNAL_SERVER_ERROR = 500`
- `HTTP_STATUS_NOT_IMPLEMENTED = 501`
- `HTTP_STATUS_BAD_GATEWAY = 502`
- `HTTP_STATUS_GATEWAY_TIMEOUT = 504`

사용 위치:

- `ResponseBuilder`가 응답을 만들 때 사용한다.
- `RequestParser`가 parser error를 표현할 때 사용한다.
- handler들이 상황에 맞는 에러 응답을 반환할 때 사용한다.
- `ConfigParser`가 `return 301`, `error_page 404` 같은 config 값을 검증할 때 사용한다.

#### `parseHttpMethod(const std::string& method)`

문자열 method를 `HttpMethod` enum으로 바꾼다.

예:

```text
"GET"    -> HTTP_METHOD_GET
"POST"   -> HTTP_METHOD_POST
"DELETE" -> HTTP_METHOD_DELETE
"PUT"    -> HTTP_METHOD_NOT_IMPLEMENTED
```

`RequestParser::parseRequestLine()`에서 사용된다.

#### `httpMethodName(HttpMethod method)`

enum method를 다시 문자열로 바꾼다.

사용 위치:

- `Allow` header 생성
- CGI 환경변수 `REQUEST_METHOD`
- debug/log 또는 응답 생성

#### `isImplementedMethod(HttpMethod method)`

서버가 실제 구현한 method인지 확인한다.

현재 true:

- GET
- POST
- DELETE

false:

- PUT
- PATCH
- OPTIONS 등

`EventLoop::prepareSuccessResponse()`에서 먼저 검사해서 구현하지 않은 method는 `501 Not Implemented`로 보낸다.

#### `reasonPhrase(int statusCode)`

status code에 대응하는 reason phrase를 반환한다.

예:

```text
200 -> "OK"
404 -> "Not Found"
502 -> "Bad Gateway"
```

`HttpResponse::setStatusCode()`와 `ResponseBuilder`에서 사용된다.

#### `isMandatoryStatusCode(int statusCode)`

현재 프로젝트에서 명시적으로 지원하는 status code인지 검사한다.

사용 위치:

- `ConfigParser::parseStatusCode()`
- CGI `Status:` header 검증

#### `isErrorStatusCode(int statusCode)`

`statusCode >= 400`인지 확인한다.

사용 위치:

- handler가 반환한 응답이 에러라면 custom error page로 교체할지 판단할 때 사용한다.

---

### include/webserv/Config.hpp

config 파일을 파싱한 결과를 담는 구조체와 directive 규칙이 정의되어 있다.

#### `enum DirectiveContext`

directive가 등장할 수 있는 위치를 bit flag로 표현한다.

```cpp
DIRECTIVE_CONTEXT_TOP
DIRECTIVE_CONTEXT_SERVER
DIRECTIVE_CONTEXT_LOCATION
```

예:

- `server`는 top level에서만 가능
- `listen`은 server block 안에서만 가능
- `methods`는 location block 안에서만 가능

#### `struct DirectiveRule`

config directive의 문법 규칙을 담는다.

필드:

- `name`: directive 이름
- `minArgs`: 최소 인자 수
- `maxArgs`: 최대 인자 수
- `contexts`: 허용 위치
- `duplicateAllowed`: 같은 block에서 중복 허용 여부
- `inheritedByLocation`: location으로 상속되는 성격인지 표시

사용 위치:

- `defaultDirectiveRules()`에서 전체 directive 규칙 목록을 만든다.
- `ConfigParser`가 알 수 없는 directive, 위치가 잘못된 directive, 중복 directive를 검증할 때 기준이 된다.

#### `struct ListenEndpoint`

listen할 주소와 port를 담는다.

필드:

- `host`
- `port`

예:

```conf
listen 127.0.0.1:8080;
```

이 설정은 다음처럼 저장된다.

```text
host = "127.0.0.1"
port = 8080
```

사용 위치:

- `Server::createListenSocket()`이 실제 socket을 만들 때 사용한다.

#### `struct LocationConfig`

`location` block 하나의 설정을 담는다.

중요한 필드:

- `path`: location prefix
- `root`: location root
- `rootSet`: location에서 root를 직접 설정했는지
- `indexes`: index 파일 후보 목록
- `indexesSet`: location에서 index를 직접 설정했는지
- `methods`: 허용 method 목록
- `methodsSet`: location에서 methods를 직접 설정했는지
- `autoindex`: directory listing on/off
- `uploadEnabled`: upload 허용 여부
- `uploadStore`: upload 저장 경로
- `cgiByExtension`: `.py -> /usr/bin/python3` 같은 CGI map
- `redirectCode`, `redirectTarget`: redirect 설정
- `clientMaxBodySize`: body size 제한
- `errorPages`: location별 custom error page

`rootSet`, `indexesSet` 같은 `*Set` 필드가 중요한 이유:

```text
server root는 기본값으로 location에 상속되어야 한다.
하지만 location이 root를 직접 지정했다면 location root가 우선이다.
```

즉, 값 자체와 “직접 지정했는지”를 분리해서 상속 처리를 정확히 한다.

#### `struct ServerConfig`

`server` block 하나의 설정을 담는다.

필드:

- `listen`: 여러 listen endpoint
- `root`: server 기본 root
- `indexes`: 기본 index 파일 목록
- `clientMaxBodySize`: 기본 body limit
- `errorPages`: server level custom error page
- `locations`: location config 목록

사용 위치:

- `Server`가 listen socket을 열 때 사용한다.
- `Router`가 요청 URI에 맞는 location을 찾고 effective config를 만들 때 사용한다.

#### `defaultDirectiveRules()`

지원하는 directive 목록과 규칙을 만든다.

지원 directive:

- `server`
- `listen`
- `root`
- `index`
- `client_max_body_size`
- `error_page`
- `location`
- `methods`
- `return`
- `autoindex`
- `upload`
- `upload_store`
- `cgi`

#### `defaultListenEndpoint()`

기본 listen 값을 만든다.

현재 기본:

```text
127.0.0.1:8080
```

#### `defaultLocationConfig()`

location 기본값을 만든다.

예:

- path `/`
- default index `index.html`
- default method `GET`
- autoindex off
- upload off
- redirect 없음

#### `defaultServerConfig()`

server 기본값을 만든다.

예:

- listen `127.0.0.1:8080`
- root `./www`
- index `index.html`
- body limit `10MB`

#### `directiveAllowedInContext()`

directive 규칙의 `contexts` bit flag와 현재 context를 비교한다.

사용 예:

```text
listen directive가 location block에 나오면 false
methods directive가 server block에 나오면 false
```

---

### include/webserv/ConfigParser.hpp

config 파일을 읽고 `ServerConfig` 목록으로 바꾸는 parser 클래스이다.

#### `class ConfigParser`

외부에서 사용하는 공개 함수는 하나다.

```cpp
std::vector<ServerConfig> parseFile(const std::string& path);
```

역할:

1. config 파일을 읽는다.
2. token으로 나눈다.
3. `server { ... }` block을 파싱한다.
4. `ServerConfig` vector를 반환한다.

#### `struct Token`

config token 하나를 표현한다.

필드:

- `value`: token 문자열
- `line`: 몇 번째 줄에서 나온 token인지

line을 저장하는 이유:

- config 오류가 발생했을 때 `"config line N: ..."`처럼 위치를 알려주기 위해서다.

#### private parser 함수들

`readFile()`:

- 파일 전체를 문자열로 읽는다.
- 없는 파일, 열 수 없는 파일이면 예외를 던진다.

`tokenize()`:

- `{`, `}`, `;`를 독립 token으로 분리한다.
- `#` comment를 무시한다.
- line number를 기록한다.

`parseServers()`:

- top level에서 여러 `server` block을 읽는다.
- server가 하나도 없으면 실패한다.

`parseServerBlock()`:

- `server { ... }` 내부 directive를 읽어 `ServerConfig`를 만든다.
- `location`이 나오면 `parseLocationBlock()`으로 넘긴다.

`parseLocationBlock()`:

- `location /path { ... }` 내부 directive를 읽어 `LocationConfig`를 만든다.
- nested location은 허용하지 않는다.

`parseDirectiveArguments()`:

- directive 이름 뒤부터 `;` 전까지 인자 목록을 모은다.

`atEnd()`, `peek()`, `consume()`, `expect()`:

- token stream을 안전하게 읽는 helper다.

`fail()`:

- 현재 token line을 포함해 `std::runtime_error`를 던진다.

`applyServerDirective()`:

- server block에서 가능한 directive를 실제 `ServerConfig`에 적용한다.
- 예: `listen`, `root`, `index`, `client_max_body_size`, `error_page`

`applyLocationDirective()`:

- location block directive를 `LocationConfig`에 적용한다.
- 예: `methods`, `return`, `autoindex`, `upload`, `upload_store`, `cgi`

`validateLocation()`:

- upload on인데 upload_store가 없으면 실패시키는 등 location 단위 검증을 한다.

`parseListen()`:

- `"127.0.0.1:8080"`을 `ListenEndpoint`로 바꾼다.

`parseBodySize()`:

- `10M`, `1K`, `100` 같은 문자열을 byte 단위 숫자로 바꾼다.

`parseStatusCode()`:

- `404`, `500`, `301` 같은 status code를 정수로 바꾸고 유효성 검증한다.

`parseAllowedMethod()`:

- config의 `GET`, `POST`, `DELETE`를 `HttpMethod`로 바꾼다.

`parseOnOff()`:

- `on`, `off`를 bool로 바꾼다.

---

### include/webserv/Server.hpp

listen socket을 만들고 event loop를 시작하는 클래스이다.

#### `class Server`

역할:

- config에서 받은 listen endpoint마다 socket 생성
- bind/listen 수행
- 생성한 listen fd들을 `EventLoop`에 넘김
- destructor에서 listen socket 정리

#### constructors

`Server(const std::string& host, unsigned short port)`:

- 초기 단계용 단일 host/port constructor이다.
- 내부에서 기본 `ServerConfig`를 만들어 사용한다.

`Server(const ServerConfig& config)`:

- server block 하나만 실행할 때 사용한다.

`Server(const std::vector<ServerConfig>& configs)`:

- 여러 server block을 실행할 때 사용한다.
- 현재 `main.cpp`에서 이 constructor를 사용한다.

#### `run()`

listen socket 정보를 `ListenSocketConfig` vector로 만들고 `EventLoop::run()`을 호출한다.

#### `host()`, `port()`

첫 번째 server의 첫 번째 listen endpoint 정보를 반환한다.

현재는 주로 초기 단계 호환/디버깅용 성격이다.

#### `listenSocketCount()`

열린 listen socket 개수를 반환한다.

`main.cpp`에서 로그로 출력한다.

#### private `ListenBinding`

listen fd와 어떤 server config가 연결되는지 저장한다.

필드:

- `fd`: listen socket fd
- `serverIndex`: `_servers`에서 어느 server인지
- `listenIndex`: 해당 server의 몇 번째 listen인지

#### `openListenSockets()`

모든 server config의 listen endpoint를 순회하며 socket을 만든다.

중복 endpoint가 있으면 실패한다.

#### `createListenSocket()`

실제 socket 생성 흐름:

```text
socket()
-> fcntl(O_NONBLOCK)
-> setsockopt(SO_REUSEADDR)
-> bind()
-> listen()
```

#### `closeListenSockets()`

열려 있는 listen fd를 닫는다.

---

### include/webserv/EventLoop.hpp

서버의 중심이다. 모든 I/O를 하나의 `poll()` loop 안에서 처리한다.

#### `struct ListenSocketConfig`

`EventLoop`에 listen fd와 그 fd에 연결된 `ServerConfig`를 넘기기 위한 구조체이다.

#### `class EventLoop`

역할:

- listen fd 감시
- client fd 감시
- CGI pipe fd 감시
- client timeout 처리
- CGI timeout 처리
- request parsing 호출
- routing 호출
- handler 호출
- response 전송
- bonus session 관리

#### private `struct CgiJob`

실행 중인 CGI 하나의 상태를 저장한다.

필드:

- `pid`: child process pid
- `clientFd`: 이 CGI를 기다리는 client fd
- `stdinFd`: CGI stdin으로 write할 pipe fd
- `stdoutFd`: CGI stdout에서 read할 pipe fd
- `requestBody`: CGI stdin으로 보낼 body
- `stdinOffset`: body를 어디까지 write했는지
- `output`: CGI stdout 누적 buffer
- `startTime`: timeout 계산용
- `stdinClosed`, `stdoutClosed`: pipe 종료 상태
- `childReaped`: child waitpid 완료 여부
- `childStatus`: child exit status
- `errorPages`, `errorRoot`: CGI 실패 시 custom error page 생성을 위한 정보

#### private `struct Session`

STEP20 bonus session 저장 단위이다.

필드:

- `visits`: `/session` 방문 횟수
- `createdAt`: 생성 시간
- `lastAccess`: 마지막 접근 시간

#### 주요 멤버 변수

`_pollFds`:

- `poll()`에 넘길 fd 목록이다.

`_clients`:

- client fd -> `Client` 객체 map이다.

`_serversByListenFd`:

- listen fd -> `ServerConfig` map이다.
- 어떤 listen socket으로 들어온 연결인지에 따라 server config를 찾는다.

`_cgiJobs`:

- client fd -> CGI job map이다.

`_cgiClientByFd`:

- CGI pipe fd -> client fd map이다.
- poll event가 CGI pipe fd에서 발생했을 때 어느 client의 CGI인지 찾는다.

`_orphanedCgiPids`:

- client disconnect 등으로 job은 정리됐지만 child가 아직 종료되지 않은 pid 목록이다.

`_sessions`:

- session id -> Session map이다.

`_sessionCounter`:

- session id fallback 생성에 사용하는 counter이다.

#### public `run()`

서버의 main event loop이다.

반복 흐름:

```text
poll()
-> timeout/client cleanup
-> CGI timeout/check
-> listen fd event면 accept
-> client fd event면 read/write
-> CGI pipe event면 CGI read/write
```

#### fd 관리 함수

`addFd()`:

- fd를 poll list에 추가한다.

`updateEvents()`:

- 특정 fd가 감시할 event를 바꾼다.
- 예: 요청을 읽는 중이면 `POLLIN`, 응답이 준비되면 `POLLOUT`

`removeFd()`:

- fd를 poll list에서 제거한다.

`isListenFd()`:

- fd가 listen socket인지 확인한다.

`isCgiFd()`:

- fd가 CGI pipe인지 확인한다.

#### client 관리 함수

`closeClient()`:

- client fd를 닫고 `_clients`에서 제거한다.
- 연결된 CGI가 있으면 같이 정리한다.

`closeAllClients()`:

- destructor에서 모든 client를 정리할 때 사용한다.

`closeTimedOutClients()`:

- 일정 시간 활동이 없는 client를 닫는다.

#### event 처리 함수

`handleListenEvent()`:

- listen socket이 readable이면 `accept()`로 가능한 만큼 client를 받는다.
- 새 client fd를 non-blocking으로 만들고 `_clients`에 등록한다.

`handleClientRead()`:

- client fd에서 `recv()` 가능한 만큼 읽는다.
- input buffer에 누적한다.
- `RequestParser`로 request 완성 여부를 확인한다.

`handleClientWrite()`:

- output buffer에서 아직 보내지 않은 부분만 `send()`한다.
- partial send를 처리하기 위해 `sendOffset`을 사용한다.

`handleCgiEvent()`:

- CGI stdin/stdout pipe event를 해당 handler로 분배한다.

`handleCgiWrite()`:

- request body를 CGI stdin pipe에 partial write한다.
- 다 쓰면 stdin pipe를 닫아 CGI에 EOF를 보낸다.

`handleCgiRead()`:

- CGI stdout을 non-blocking read로 읽어 job output에 누적한다.
- output이 너무 크면 `502 Bad Gateway`로 실패 처리한다.

#### request 처리 함수

`processClientInput()`:

- `RequestParser::parse()`를 호출한다.
- incomplete이면 계속 읽는다.
- error이면 error response를 만든다.
- complete이면 `prepareSuccessResponse()`로 넘긴다.

`prepareEarlyBodyLimitResponse()`:

- body를 읽는 도중에도 이미 limit을 넘었는지 검사한다.
- 큰 요청을 끝까지 받기 전에 `413 Payload Too Large`를 준비할 수 있게 한다.

`prepareSuccessResponse()`:

- request가 완성된 뒤 실제 응답 종류를 결정한다.

분기 순서:

```text
server config 찾기
-> method 구현 여부 확인
-> Router::route()
-> redirect
-> method allowed
-> body limit
-> /session bonus route
-> CGI
-> /echo
-> POST upload
-> DELETE
-> static GET
```

#### session 함수

`prepareSessionResponse()`:

- `/session` GET 요청 전용이다.
- `Cookie` header에서 `WSID`를 찾는다.
- 유효한 session이면 방문 횟수를 증가시킨다.
- 없거나 잘못된 cookie면 새 session을 만든다.
- `Set-Cookie` header를 응답에 넣는다.

`purgeExpiredSessions()`:

- TTL이 지난 session을 제거한다.

`enforceSessionLimit()`:

- session 수가 최대 개수 이상이면 가장 오래 접근하지 않은 session을 제거한다.

`createSessionId()`:

- `/dev/urandom`에서 16 byte를 읽고 32자리 hex string을 만든다.
- 실패 시 fallback 생성 로직을 사용한다.

#### CGI job 함수

`startCgiResponse()`:

- `CgiHandler::start()`로 child process와 pipe를 만든다.
- stdout pipe는 `POLLIN`, stdin pipe는 body가 있으면 `POLLOUT`으로 poll에 등록한다.

`checkCgiJobs()`:

- child가 끝났는지 `waitpid(..., WNOHANG)`으로 확인한다.
- timeout이 넘으면 `504 Gateway Timeout` 처리한다.
- stdout이 닫히고 child도 회수됐으면 `completeCgiJob()`으로 응답을 만든다.

`reapOrphanedCgiPids()`:

- client disconnect 등으로 orphan 목록에 들어간 CGI child들을 계속 회수한다.

`closeCgiFd()`:

- CGI pipe fd를 poll list와 map에서 제거하고 close한다.

`cleanupCgiForClient()`:

- 특정 client의 CGI job을 정리한다.
- 필요하면 child process를 kill한다.

`failCgiJob()`:

- CGI 실패 시 pipe/child를 정리하고 client에게 error response를 준비한다.

`completeCgiJob()`:

- CGI stdout 전체를 `CgiHandler::buildResponse()`로 HTTP 응답으로 바꾼다.
- child가 비정상 종료했거나 output이 잘못되면 `502 Bad Gateway`.

#### error response 함수

`prepareErrorResponse()`:

- 기본 또는 custom error page를 사용해 error response를 client output에 넣는다.

`prepareMethodNotAllowedResponse()`:

- `405 Method Not Allowed` 응답을 만들고 `Allow` header를 추가한다.

`serverForClient()`:

- client가 들어온 listen fd를 기준으로 server config를 찾는다.

---

### include/webserv/Client.hpp

연결된 클라이언트 하나의 상태와 buffer를 관리한다.

#### `class Client`

필드 의미:

- `_fd`: client socket fd
- `_listenFd`: 이 client를 accept한 listen fd
- `_inputBuffer`: 아직 처리하지 않은 수신 데이터
- `_outputBuffer`: 아직 전송하지 않은 응답 데이터
- `_sendOffset`: output buffer에서 어디까지 보냈는지
- `_request`: 현재 파싱 중이거나 완성된 HTTP request
- `_parser`: request parser 상태 기계
- `_state`: client 상태
- `_lastActivity`: timeout 계산용 마지막 활동 시간
- `_closing`: 닫을 예정인지

#### accessor 함수

`fd()`, `listenFd()`, `state()`, `lastActivity()`:

- EventLoop가 client 상태를 확인할 때 사용한다.

`inputBuffer()`, `outputBuffer()`, `sendOffset()`:

- read/write 처리와 debug에 사용된다.

`request()`, `parser()`:

- EventLoop가 request와 parser에 접근할 때 사용한다.

#### buffer 함수

`appendInput()`:

- `recv()`로 읽은 bytes를 input buffer 뒤에 붙인다.
- 활동 시간을 갱신한다.

`consumeInput()`:

- parser가 처리한 bytes만큼 input buffer 앞부분을 제거한다.

`setOutput()`:

- serialize된 HTTP response를 output buffer에 넣고 state를 writing으로 바꾼다.

`advanceSendOffset()`:

- `send()` 성공한 byte 수만큼 offset을 증가시킨다.

`pendingOutputSize()`, `pendingOutputData()`:

- 아직 보내지 않은 응답 조각을 반환한다.

`outputComplete()`:

- 응답을 다 보냈는지 확인한다.

#### state 함수

`setState()`:

- client state를 바꾸고 활동 시간을 갱신한다.

`markClosing()`:

- client를 닫을 상태로 표시한다.

`touch()`:

- 마지막 활동 시간을 현재 시간으로 갱신한다.

`desiredPollEvents()`:

- 현재 client 상태에 따라 poll event를 결정한다.

예:

```text
읽는 중 -> POLLIN
응답 보낼 것 있음 -> POLLOUT
CGI 실행 중 -> 0 또는 CGI fd가 따로 감시됨
닫는 중 -> 0
```

---

### include/webserv/HttpRequest.hpp

파싱된 HTTP request를 담는 데이터 클래스이다.

#### `enum BodyFraming`

body가 어떤 방식으로 들어왔는지 표시한다.

- `BODY_NONE`
- `BODY_CONTENT_LENGTH`
- `BODY_CHUNKED`

#### setter 함수

`clear()`:

- request를 초기 상태로 되돌린다.

`setMethodText()`:

- 원본 method 문자열을 저장한다.

`setMethod()`:

- enum method를 저장한다.

`setRawTarget()`:

- request line의 원본 target을 저장한다.
- 예: `/cgi-bin/hello.py?name=abc`

`setPath()`:

- query string이 제거된 path를 저장한다.

`setQuery()`:

- `?` 뒤 query string을 저장한다.

`setVersion()`:

- `HTTP/1.1` 같은 version을 저장한다.

`addHeader()`:

- header 이름을 lowercase로 저장한다.
- 같은 이름 header가 중복되면 `", "`로 이어 붙인다.

`setBody()`:

- request body를 저장한다.
- chunked request는 decoded body가 들어간다.

`setBodyFraming()`:

- body framing 방식을 저장한다.

`setContentLength()`:

- Content-Length 값을 저장하고 framing을 `BODY_CONTENT_LENGTH`로 설정한다.

`setErrorStatus()`:

- parser가 발견한 error status를 request에도 기록한다.

#### getter 함수

`methodText()`, `method()`, `rawTarget()`, `path()`, `query()`, `version()`, `body()`:

- handler, router, CGI에서 request 정보를 읽을 때 사용한다.

`bodyFraming()`:

- EventLoop가 body limit을 조기 검사할 때 사용한다.

`hasContentLength()`, `contentLength()`:

- body size 검사에 사용한다.

`headers()`:

- CGI 환경변수 생성 시 전체 header를 순회할 때 사용한다.

`hasHeader()`, `header()`:

- 특정 header 값만 필요할 때 사용한다.
- header 이름은 case-insensitive로 찾는다.

`errorStatus()`:

- parser error 상태 확인용이다.

---

### include/webserv/RequestParser.hpp

HTTP request를 byte stream에서 구조화된 `HttpRequest`로 바꾸는 parser이다.

#### `enum Result`

파싱 결과를 표현한다.

- `PARSE_INCOMPLETE`: 아직 데이터가 더 필요함
- `PARSE_COMPLETE`: request 하나 완성
- `PARSE_ERROR`: 잘못된 요청

#### `parse(const std::string& input, HttpRequest& request)`

EventLoop가 client input buffer를 넘기는 진입점이다.

역할:

1. request line 파싱
2. header 파싱
3. Content-Length body 파싱
4. chunked body 파싱
5. 완료 시 consumed byte 수 기록

#### `state()`

현재 parser state를 반환한다.

EventLoop는 이 값을 보고 header 읽는 중인지 body 읽는 중인지 판단한다.

#### `consumedBytes()`

완성된 request가 input buffer에서 몇 byte를 사용했는지 반환한다.

`Client::consumeInput()`에서 사용된다.

#### `errorStatus()`

파싱 실패 시 반환해야 할 HTTP status code를 알려준다.

#### `reset()`

request 하나 처리 후 parser 상태를 초기화한다.

#### private 함수

`parseRequestLine()`:

- `GET /path HTTP/1.1`을 method, target, version으로 나눈다.
- URI 길이와 HTTP version을 검증한다.

`parseHeaders()`:

- header block을 line 단위로 파싱한다.
- Host header, Content-Length, Transfer-Encoding 등을 검증한다.

`parseChunkedBody()`:

- chunk size line을 읽고 chunk data를 decoded body에 누적한다.
- malformed chunk는 `400 Bad Request`로 처리한다.

`setError()`:

- parser state를 error로 바꾸고 status code를 저장한다.

---

### include/webserv/HttpResponse.hpp

HTTP response를 표현하고 wire format 문자열로 직렬화한다.

#### setter 함수

`setVersion()`:

- 보통 `HTTP/1.1`을 사용한다.

`setStatusCode()`:

- status code와 reason phrase를 설정한다.

`setReasonPhrase()`:

- CGI `Status:` header처럼 custom reason phrase가 필요할 때 사용한다.

`setHeader()`:

- header를 추가하거나 같은 이름 header를 교체한다.
- header 이름 비교는 case-insensitive다.

`setBody()`:

- body 문자열을 저장한다.

`setConnectionClose()`:

- `Connection: close` 또는 keep-alive 값을 결정한다.

#### getter 함수

`statusCode()`, `body()`, `connectionClose()` 등은 handler 결과 판단과 테스트에 사용된다.

#### `serialize()`

HTTP 응답 문자열을 만든다.

생성 형식:

```http
HTTP/1.1 200 OK
Header: value
Content-Length: ...
Connection: close

body
```

`Content-Length`가 없으면 자동으로 넣는다.  
`204 No Content`처럼 body가 없어야 하는 status는 body를 출력하지 않는다.

---

### include/webserv/ResponseBuilder.hpp

자주 만드는 응답을 쉽게 생성하는 factory 클래스이다.

#### `text()`

plain/text/html 등 body가 있는 일반 응답을 만든다.

사용 예:

- `/echo`
- upload 성공 응답
- session bonus 응답

#### `error(int statusCode)`

기본 error page body를 가진 에러 응답을 만든다.

#### `error(int statusCode, body, contentType)`

custom error page body를 가진 에러 응답을 만든다.

`EventLoop::prepareErrorResponse()`에서 사용된다.

#### `redirect()`

`Location` header가 포함된 redirect 응답을 만든다.

#### `methodNotAllowed()`

`405`와 `Allow` header를 가진 응답을 만든다.

현재 EventLoop는 직접 `prepareMethodNotAllowedResponse()`를 사용하지만, 이 함수는 응답 builder helper로 남아 있다.

---

### include/webserv/Router.hpp

request URI와 config를 연결한다.

#### `struct EffectiveRouteConfig`

server config와 location config를 합친 최종 설정이다.

예:

```text
server root = ./www
location /cgi-bin root = ./www/cgi-bin
```

요청이 `/cgi-bin/hello.py`라면 effective root는 `./www/cgi-bin`이 된다.

필드:

- root
- indexes
- methods
- autoindex
- uploadEnabled
- uploadStore
- cgiByExtension
- redirectCode
- redirectTarget
- clientMaxBodySize
- errorPages

#### `struct RouteResult`

라우팅 결과 전체를 담는다.

필드:

- `ok`: 라우팅 성공 여부
- `statusCode`: 실패 시 반환할 status
- `uriPath`: 정규화된 URI path
- `queryString`: query string
- `locationPrefix`: 선택된 location path
- `relativePath`: location 기준 상대 경로
- `filesystemPath`: 실제 파일 경로
- `effective`: 최종 route config

#### `Router::route()`

핵심 라우팅 함수이다.

역할:

1. request URI를 path policy로 검증한다.
2. 가장 긴 prefix를 가진 location을 선택한다.
3. server config와 location config를 merge한다.
4. 실제 filesystem path를 계산한다.
5. `RouteResult`를 반환한다.

사용 위치:

- `EventLoop::prepareSuccessResponse()`
- `EventLoop::prepareEarlyBodyLimitResponse()`

---

### include/webserv/PathPolicy.hpp

URI를 안전한 filesystem path로 바꾸는 정책 함수가 정의되어 있다.

#### `enum PathPolicyError`

path 변환 실패 이유:

- `PATH_POLICY_BAD_URI`
- `PATH_POLICY_BAD_ESCAPE`
- `PATH_POLICY_TRAVERSAL`
- `PATH_POLICY_LOCATION_MISMATCH`

#### `struct PathResolution`

path 변환 결과:

- `ok`
- `error`
- `uriPath`
- `relativePath`
- `filesystemPath`

#### `stripQueryString()`

URI에서 `?query` 부분을 제거한다.

#### `resolveUriPath()`

request URI를 검증하고 실제 파일 경로로 바꾼다.

처리:

1. query string 제거
2. percent decoding
3. `/../` traversal 차단
4. location prefix와 맞는지 확인
5. root와 relative path를 합쳐 filesystem path 생성

#### `pathPolicyErrorName()`

path policy error를 문자열로 바꾼다.

---

### include/webserv/StaticFileHandler.hpp

GET 요청으로 파일 또는 디렉토리를 제공하는 handler이다.

#### constructor

`StaticFileHandler(const std::string& root)`:

- 정적 파일 기준 root를 저장한다.

#### `handleGet()`

초기 단계용 함수이다.  
request path와 root를 조합해 `handlePath()`로 처리한다.

#### `handlePath()`

EventLoop에서 실제 사용하는 함수이다.

인자:

- `path`: 실제 filesystem path
- `indexes`: index 후보 목록
- `autoindex`: directory listing 허용 여부
- `uriPath`: autoindex 링크와 slash redirect에 사용할 URI

#### private 함수

`buildFileResponse()`:

- 일반 파일을 읽어 body로 만들고 MIME type을 설정한다.

`buildDirectoryResponse()`:

- directory 요청 처리.
- trailing slash가 없으면 `301` redirect.
- index 파일이 있으면 index 파일 응답.
- autoindex on이면 directory listing.
- autoindex off이면 `403`.

`buildAutoindexResponse()`:

- directory 목록을 HTML로 만든다.
- 파일명 HTML escape와 URI encode를 수행한다.

---

### include/webserv/UploadHandler.hpp

POST upload를 처리하는 handler이다.

#### constructor

`UploadHandler(const std::string& uploadStore)`:

- 저장 디렉토리를 저장한다.

#### `handlePost()`

upload 요청 처리 진입점이다.

처리:

- upload store가 유효한지 확인
- `multipart/form-data`면 multipart parser 사용
- 아니면 raw body upload로 처리

#### private `SavedFile`

저장된 파일명과 새로 생성됐는지 여부를 기록한다.

이 값으로 응답 status를 결정한다.

```text
새 파일 있음 -> 201 Created
모두 overwrite -> 200 OK
```

#### private 함수

`handleRawBody()`:

- body 전체를 자동 생성 파일명으로 저장한다.

`handleMultipart()`:

- boundary를 기준으로 multipart body를 나눈다.
- 각 part의 `Content-Disposition`에서 filename을 찾는다.
- filename 검증 후 파일 저장.

`buildSuccessResponse()`:

- `created filename` 또는 `updated filename` body를 만든다.
- 첫 파일에 대한 `Location` header를 설정한다.

`ensureUploadStore()`:

- upload 저장소가 존재하는 directory이고 write/execute 권한이 있는지 확인한다.

`saveFile()`:

- `open(O_CREAT | O_TRUNC)`로 파일을 저장한다.
- 저장 전 존재 여부를 확인해 created/updated를 구분한다.

---

### include/webserv/DeleteHandler.hpp

DELETE 요청을 처리하는 handler이다.

#### `handleDelete(filesystemPath, relativePath)`

처리:

1. path traversal 가능성이 있는 relative path를 거절한다.
2. 대상 파일 stat 확인.
3. directory면 거절.
4. regular file이 아니면 거절.
5. `std::remove()`로 삭제.
6. 성공 시 `204 No Content`.

---

### include/webserv/CgiHandler.hpp

CGI 실행을 담당한다.

#### `struct CgiExecution`

CGI child process 시작 결과를 담는다.

필드:

- `ok`: 시작 성공 여부
- `statusCode`: 실패 시 사용할 HTTP status
- `pid`: child pid
- `stdinFd`: parent가 CGI stdin에 write할 fd
- `stdoutFd`: parent가 CGI stdout에서 read할 fd
- `requestBody`: CGI stdin으로 전달할 body

#### `CgiHandler::isCgiRequest()`

route의 filesystem path 확장자를 보고 CGI 대상인지 판단한다.

예:

```text
hello.py -> .py가 config cgi map에 있으면 true
hello.sh -> .sh가 config cgi map에 있으면 true
index.html -> false
```

#### `CgiHandler::start()`

CGI process와 pipe를 준비한다.

흐름:

```text
extension으로 interpreter 찾기
-> script stat 검증
-> stdin/stdout pipe 생성
-> fork
-> child: dup2, chdir, execve
-> parent: pipe fd non-blocking 설정
-> CgiExecution 반환
```

#### `CgiHandler::buildResponse()`

CGI stdout 전체를 HTTP response로 바꾼다.

처리:

- header/body separator 찾기
- `Status:` header 파싱
- `Content-Type` 또는 `Location` header 확인
- 금지 header(`Content-Length`, `Connection`, `Transfer-Encoding`)는 무시
- body 설정

---

### include/webserv/ErrorPageHandler.hpp

에러 응답 body를 만든다.

#### `defaultErrorPage()`

기본 HTML error page를 만든다.

#### `loadCustomErrorPage()`

config의 `error_page` 설정을 보고 custom error file을 읽는다.

성공하면:

- `body`
- `contentType`

을 채우고 true를 반환한다.

실패하면 false를 반환하고 caller가 default error page를 사용한다.

---

### include/webserv/MimeTypes.hpp

#### `mimeTypeForPath()`

파일 확장자에 맞는 Content-Type을 반환한다.

예:

- `.html` -> `text/html`
- `.css` -> `text/css`
- `.svg` -> `image/svg+xml`
- unknown -> `application/octet-stream`

`StaticFileHandler::buildFileResponse()`에서 사용된다.

---

### include/webserv/Signal.hpp

서버 종료 signal을 다룬다.

#### `handleShutdownSignal()`

`SIGINT`, `SIGTERM`이 들어오면 shutdown flag를 켠다.

#### `shutdownRequested()`

EventLoop가 loop 조건에서 확인한다.

---

### include/webserv/StateMachine.hpp

client 상태와 parser 상태를 enum으로 표현한다.

#### `enum ClientState`

- `CLIENT_READING_HEADERS`
- `CLIENT_READING_BODY`
- `CLIENT_PROCESSING_REQUEST`
- `CLIENT_RUNNING_CGI`
- `CLIENT_WRITING_RESPONSE`
- `CLIENT_CLOSING`

#### `enum RequestParserState`

- `PARSER_REQUEST_LINE`
- `PARSER_HEADERS`
- `PARSER_BODY_BY_LENGTH`
- `PARSER_BODY_CHUNK_SIZE`
- `PARSER_BODY_CHUNK_DATA`
- `PARSER_BODY_CHUNK_CRLF`
- `PARSER_BODY_CHUNK_TRAILERS`
- `PARSER_COMPLETE`
- `PARSER_ERROR`

#### helper 함수

`clientStateName()`, `parserStateName()`:

- enum을 debug-friendly 문자열로 바꾼다.

`clientStateCanRead()`:

- client socket에서 읽을 수 있는 상태인지 확인한다.

`clientStateCanWrite()`:

- client socket에 쓸 수 있는 상태인지 확인한다.

`parserStateIsTerminal()`:

- parser가 complete/error 상태인지 확인한다.

---

## 3. 구현 파일별 역할과 함수 설명

### src/main.cpp

프로그램 진입점이다.

#### `kDefaultConfigPath`

인자 없이 실행했을 때 사용할 기본 config path이다.

#### `programName()`

usage 출력용 프로그램 이름을 가져온다.

#### `printUsage()`

인자 개수가 잘못됐을 때 사용법을 stderr로 출력한다.

#### `selectConfigPath()`

`argc == 1`이면 기본 config를 사용하고, `argc == 2`이면 argv[1]을 사용한다.

#### `main()`

전체 서버 시작 흐름:

```text
인자 개수 확인
-> signal handler 설정
-> ConfigParser로 config file 파싱
-> Server 생성
-> Server::run()
```

예외가 발생하면 `webserv: fatal: ...`을 출력하고 `1`을 반환한다.

---

### src/config/Config.cpp

config 기본값과 directive 규칙을 구현한다.

#### `DirectiveRule::DirectiveRule()`

directive 규칙 객체를 초기화한다.

#### `defaultDirectiveRules()`

지원 directive 목록을 만든다.

중요한 규칙:

- `server`: top level에서만 가능, 중복 가능
- `listen`: server에서 가능, 중복 가능
- `root`, `index`, `client_max_body_size`: server/location 가능
- `error_page`: server/location 가능, 중복 가능
- `location`: server 안에서 가능
- `methods`, `return`, `autoindex`, `upload`, `upload_store`, `cgi`: location 안에서 가능

#### `defaultListenEndpoint()`

기본 `127.0.0.1:8080` endpoint를 반환한다.

#### `defaultLocationConfig()`

location 기본값을 반환한다.

#### `defaultServerConfig()`

server 기본값을 반환한다.

#### `directiveAllowedInContext()`

directive가 현재 context에서 허용되는지 bit flag로 검사한다.

---

### src/config/ConfigParser.cpp

config parser의 실제 구현이다.

#### anonymous namespace helper

`vectorContains()`:

- 중복 directive 검사용 vector 검색 helper.

`isNumber()`:

- 문자열이 숫자로만 구성됐는지 확인한다.

`parseUnsignedLong()`:

- 문자열을 unsigned long으로 안전하게 변환한다.

`isValidIpv4Host()`:

- listen host가 IPv4 숫자 형태인지 확인한다.

`isDirectiveKeyword()`:

- directive 이름으로 사용되는 문자열인지 확인한다.

`linePrefix()`:

- error message에 line 정보를 붙인다.

#### `ConfigParser::parseFile()`

config parser의 공개 진입점이다.

흐름:

```text
readFile()
-> tokenize()
-> parseServers()
```

#### `readFile()`

파일을 열고 전체 내용을 string으로 읽는다.

#### `tokenize()`

config 문서를 token vector로 바꾼다.

특징:

- `{`, `}`, `;`는 별도 token
- `#` 이후 주석 무시
- 줄 번호 보존

#### `parseServers()`

top level에서 `server` block들을 파싱한다.

#### `parseServerBlock()`

server 내부 directive를 처리한다.

`location` directive를 만나면 `parseLocationBlock()` 호출.

#### `parseLocationBlock()`

location prefix와 내부 directive를 처리한다.

#### `parseDirectiveArguments()`

`;` 전까지 directive 인자를 수집한다.

#### `atEnd()`, `peek()`, `consume()`, `expect()`

token stream 탐색 helper.

#### `fail()`

line number를 포함한 예외를 던진다.

#### `applyServerDirective()`

server directive별 처리:

- `listen`: endpoint 추가
- `root`: root 설정
- `index`: index list 설정
- `client_max_body_size`: byte 변환 후 저장
- `error_page`: status -> URI map에 저장

#### `applyLocationDirective()`

location directive별 처리:

- `root`
- `index`
- `client_max_body_size`
- `error_page`
- `methods`
- `return`
- `autoindex`
- `upload`
- `upload_store`
- `cgi`

`cgi`는 중복 허용되어 `.py`, `.sh`처럼 여러 extension을 map에 넣을 수 있다.

#### `validateLocation()`

대표 검증:

- upload가 on인데 upload_store가 없으면 실패.

#### `parseListen()`

`host:port` 형식을 파싱한다.

#### `parseBodySize()`

`K`, `M` suffix를 byte로 환산한다.

#### `parseStatusCode()`

status code를 숫자로 바꾸고 서버가 아는 code인지 검사한다.

#### `parseAllowedMethod()`

GET/POST/DELETE만 허용한다.

#### `parseOnOff()`

`on`, `off`만 bool로 인정한다.

---

### src/core/Server.cpp

listen socket 생성과 EventLoop 시작을 담당한다.

#### helper 함수

`systemError()`:

- `errno` 기반 error message를 만든다.

`closeFd()`:

- fd가 유효하면 close한다.

`setNonBlocking()`:

- `fcntl()`로 fd에 `O_NONBLOCK`을 추가한다.

`addressForHost()`:

- IPv4 문자열을 `in_addr_t`로 변환한다.

`endpointKey()`:

- 중복 listen endpoint 검사용 key를 만든다.

#### `Server::ListenBinding`

listen fd와 server config index를 묶는다.

#### constructors

config를 `_servers`에 저장하고 `openListenSockets()`를 호출한다.

#### `~Server()`

listen socket을 닫는다.

#### `run()`

listen binding을 `ListenSocketConfig`로 바꿔 EventLoop를 생성하고 실행한다.

#### `openListenSockets()`

모든 config의 모든 listen endpoint에 대해 socket을 연다.

#### `createListenSocket()`

실제 listen socket 생성 함수이다.

#### `closeListenSockets()`

서버 종료 시 listen socket들을 닫는다.

---

### src/core/EventLoop.cpp

가장 중요한 구현 파일이다.  
실제 서버 동작 대부분이 여기에 모인다.

#### 주요 상수

- `kBufferSize`: recv/read 임시 buffer 크기
- `kMaxClients`: 최대 client 수
- `kMaxRawInputBufferSize`: raw input buffer 상한
- `kMaxCgiOutputSize`: CGI output 상한
- `kMaxSessions`: session bonus 최대 개수
- `kPollTimeoutMs`: poll timeout
- `kClientTimeoutSeconds`: client timeout
- `kCgiTimeoutSeconds`: CGI timeout
- `kSessionTtlSeconds`: session TTL

#### anonymous namespace helper

`methodAllowed()`:

- request method가 route의 allowed methods에 있는지 확인한다.

`methodNames()`:

- `Allow` header용 method 문자열을 만든다.

`configuredErrorResponse()`:

- custom error page가 있으면 읽고, 없으면 default error page를 만든다.

`bodyTooLarge()`:

- request body가 route limit을 넘었는지 검사한다.

`parserStateReadsBody()`:

- parser가 body 읽는 상태인지 확인한다.

`cookieValue()`:

- Cookie header에서 특정 cookie 값을 찾는다.

`isValidSessionId()`:

- `WSID`가 32자리 lowercase hex인지 검사한다.

`fillRandomBytes()`, `fillFallbackBytes()`, `hexEncode()`:

- session id 생성에 사용된다.

#### EventLoop 생성/소멸

`EventLoop::EventLoop()`:

- listen fd를 poll list에 등록한다.
- listen fd -> server config map을 만든다.

`EventLoop::~EventLoop()`:

- client, CGI job, orphan CGI pid를 정리한다.

#### `run()`

서버의 event loop이다.

핵심 원칙:

```text
read/write/send/recv는 poll readiness 확인 후 수행한다.
```

#### fd/client/CGI/session 함수

각 함수의 자세한 역할은 헤더 설명의 EventLoop 부분을 참고하면 된다.  
구현 파일에서는 실제 system call 처리, errno 처리, map 갱신, response 준비가 이루어진다.

---

### src/core/Client.cpp

Client 객체의 getter/setter와 buffer 관리 구현이다.

#### constructor

fd, listenFd를 저장하고 초기 상태를 `CLIENT_READING_HEADERS`로 둔다.

#### `appendInput()`

recv data를 input buffer에 누적한다.

#### `consumeInput()`

parser가 처리한 만큼 input buffer 앞부분을 제거한다.

#### `setOutput()`

응답 문자열을 output buffer에 넣고 send offset을 0으로 초기화한다.

#### `desiredPollEvents()`

현재 client 상태에 따라 EventLoop가 poll에서 감시할 event를 반환한다.

---

### src/core/StateMachine.cpp

enum 상태를 문자열과 bool helper로 바꾼다.

#### `clientStateName()`

ClientState를 사람이 읽을 수 있는 문자열로 변환한다.

#### `clientStateCanRead()`

header/body 읽기 상태이면 true.

#### `clientStateCanWrite()`

response writing 상태이면 true.

#### `parserStateName()`

RequestParserState를 문자열로 변환한다.

#### `parserStateIsTerminal()`

parser가 complete/error 상태이면 true.

---

### src/core/Signal.cpp

graceful shutdown flag를 관리한다.

#### `handleShutdownSignal()`

signal handler이다.  
실제 복잡한 정리는 하지 않고 flag만 세운다.

#### `shutdownRequested()`

EventLoop loop 조건에서 호출한다.

---

### src/http/Http.cpp

HTTP method/status helper 구현이다.

함수 역할은 `Http.hpp` 설명과 같다.

---

### src/http/HttpRequest.cpp

`HttpRequest`의 setter/getter 구현이다.

중요한 점:

- header 이름을 lowercase로 저장한다.
- header lookup도 lowercase로 수행한다.
- 중복 header는 `", "`로 이어 붙인다.

---

### src/http/RequestParser.cpp

HTTP parser 구현이다.

#### helper 함수

`isTokenChar()`, `isValidToken()`:

- method/header name에 사용할 수 있는 문자 검증.

`trim()`:

- header value 앞뒤 공백 제거.

`lowerString()`:

- header 이름 비교용 lowercase 변환.

`parseSizeValue()`:

- Content-Length 숫자 파싱.

`parseChunkSizeLine()`:

- chunk size line을 hex 숫자로 파싱.

#### `parse()`

input buffer에서 request 하나를 파싱한다.

특징:

- header 최대 크기 제한
- target 최대 길이 제한
- Content-Length와 chunked 처리
- complete 시 consumed byte 기록

#### `parseRequestLine()`

request line 검증:

- method token
- target
- HTTP version
- Host 필요 여부는 header 단계에서 처리

#### `parseHeaders()`

header 검증:

- colon 필요
- header name token 검증
- Host header 확인
- Content-Length 중복/형식 확인
- Transfer-Encoding chunked 확인

#### `parseChunkedBody()`

chunked body를 decoded body로 만든다.

---

### src/http/HttpResponse.cpp

`HttpResponse` 구현이다.

#### `setHeader()`

동일 header가 있으면 교체하고 없으면 추가한다.

#### `serialize()`

status line, headers, blank line, body를 하나의 문자열로 만든다.

---

### src/http/ResponseBuilder.cpp

응답 생성 helper 구현이다.

#### `normalizeStatus()`

알 수 없는 status는 `500 Internal Server Error`로 바꾼다.

#### `text()`, `error()`, `redirect()`, `methodNotAllowed()`

자주 쓰는 응답 형태를 만든다.

---

### src/http/Router.cpp

라우팅 구현이다.

#### helper 함수

`locationMatches()`:

- URI path가 location prefix에 매칭되는지 확인한다.

`statusForPathError()`:

- path policy error를 HTTP status로 바꾼다.

`selectLocation()`:

- longest-prefix matching으로 가장 구체적인 location을 고른다.

`buildEffectiveConfig()`:

- server 설정과 location 설정을 합친다.

#### `Router::route()`

request target을 최종 filesystem path와 effective config로 바꾼다.

---

### src/handlers/StaticFileHandler.cpp

정적 파일과 디렉토리 응답 구현이다.

#### helper 함수

`joinPath()`:

- directory와 child 이름을 안전하게 연결한다.

`htmlEscape()`:

- autoindex HTML에서 파일명을 안전하게 출력한다.

`uriEncode()`:

- autoindex 링크 href를 안전하게 만든다.

`directoryUriBase()`:

- autoindex 링크 기준 URI를 만든다.

#### `handlePath()`

파일이면 `buildFileResponse()`, 디렉토리면 `buildDirectoryResponse()`.

#### `buildFileResponse()`

파일 내용을 읽고 MIME type을 설정한다.

#### `buildDirectoryResponse()`

slash redirect, index file, autoindex, forbidden 처리를 담당한다.

#### `buildAutoindexResponse()`

directory entry를 읽고 HTML listing을 만든다.

---

### src/handlers/UploadHandler.cpp

upload 구현이다.

#### helper 함수

`multipartBoundary()`:

- `Content-Type`에서 boundary 추출.

`contentDispositionHeader()`:

- part header에서 Content-Disposition line 추출.

`headerParameter()`:

- `filename=...` 같은 parameter 추출.

`isValidFilename()`:

- 위험한 filename 거절.

`writeAll()`:

- 파일에 전체 body를 끝까지 write한다.

#### `handlePost()`

multipart와 raw body를 분기한다.

#### `handleMultipart()`

multipart body를 boundary 기준으로 파싱하고 파일 저장.

#### `saveFile()`

파일을 저장하고 created/updated 여부를 반환한다.

---

### src/handlers/DeleteHandler.cpp

DELETE 구현이다.

#### `handleDelete()`

relative path traversal을 막고, regular file만 삭제한다.

---

### src/handlers/CgiHandler.cpp

CGI 구현이다.

#### helper 함수

`buildEnvironment()`:

- CGI 환경변수를 만든다.

`childExec()`:

- child process에서 chdir 후 execve를 호출한다.

`parseCgiHeaders()`:

- CGI output header를 HTTP response header로 변환한다.

#### `CgiHandler::start()`

CGI child와 pipe를 만든다.

#### `CgiHandler::buildResponse()`

CGI stdout을 HTTP response로 변환한다.

---

### src/handlers/ErrorPageHandler.cpp

error page 구현이다.

#### `defaultErrorPage()`

간단한 HTML error page를 만든다.

#### `loadCustomErrorPage()`

config에 등록된 error page file을 읽는다.

---

### src/utils/MimeTypes.cpp

확장자별 MIME type lookup 구현이다.

#### `mimeTypeForPath()`

정적 파일 응답의 Content-Type을 결정한다.

---

### src/utils/PathPolicy.cpp

URI path 안전성 검증 구현이다.

#### helper 함수

`percentDecode()`:

- `%2F` 같은 escape를 문자로 바꾼다.

`splitSlash()`:

- path를 `/` 기준 component로 나눈다.

`makeError()`:

- PathResolution error 객체를 만든다.

`joinPath()`:

- root와 relative path를 합친다.

`locationMatches()`:

- path가 location prefix에 맞는지 확인한다.

#### `stripQueryString()`

query 제거.

#### `resolveUriPath()`

path normalize, traversal 차단, filesystem path 생성.

#### `pathPolicyErrorName()`

debug용 error 문자열 변환.

---

## 4. config, tests, www 역할

### config/

- `default.conf`: 기본 실행 config
- `multiple_ports.conf`: 여러 port listen 검증
- `step18.conf`: mandatory 통합 테스트용 config
- `step19.conf`: 평가 시연용 config
- `step20.conf`: bonus 시연용 config
- `invalid/*.conf`: parser 실패 테스트용 잘못된 config들

### tests/

- `integration.sh`: 전체 테스트 진입점
- `http_utils.py`: 서버 실행, raw HTTP, response parser, assertion helper
- `config_suite.py`: config 정상/오류 검증
- `integration_suite.py`: static, routing, upload, DELETE 검증
- `malformed.py`: malformed request 후 서버 생존 검증
- `chunked.py`: chunked body 검증
- `slow_client.py`: 느린 client 검증
- `cgi_nonblocking.py`: CGI non-blocking 검증
- `stress.py`: concurrent request stress 검증
- `valgrind_smoke.py`: memory/fd smoke 검증
- `bonus.py`: cookie/session과 여러 CGI type 검증

### www/

- `index.html`, `style.css`, `images/`: 정적 사이트
- `upload.html`: upload demo form
- `errors/`: custom error page
- `uploads/`: upload 저장소
- `cgi-bin/hello.py`: Python CGI demo
- `cgi-bin/hello.sh`: shell CGI demo
- `cgi-bin/timeout.py`: CGI timeout 테스트
- `cgi-bin/bad_output.py`: 잘못된 CGI output 테스트
- `listing/`, `has-index/`, `private-directory/`: autoindex/index 테스트 fixture
- `site_b/`: 두 번째 port에서 다른 content 제공

---

## 5. 평가 중 설명하기 좋은 핵심 질문

### 왜 non-blocking이어야 하는가?

느린 client 하나가 `recv()`나 `send()`에서 서버 전체를 멈추게 하면 안 된다.  
그래서 listen/client/CGI pipe fd를 모두 non-blocking으로 만들고 `poll()` readiness가 온 fd만 처리한다.

### request 완료를 어떻게 판단하는가?

`RequestParser`가 header terminator `\r\n\r\n`을 찾고, body framing에 따라 필요한 body 길이를 확인한다.

- Content-Length: 지정된 byte 수만큼 body가 있어야 완료
- chunked: `0\r\n\r\n`까지 읽고 decoded body를 만들어야 완료

### routing은 어떻게 하는가?

`Router::route()`가 URI path를 normalize하고, `server.locations` 중 가장 긴 prefix를 선택한다.  
그 다음 server config와 location config를 합친 `EffectiveRouteConfig`를 만든다.

### static file은 어디서 처리하는가?

`EventLoop::prepareSuccessResponse()`에서 GET 요청이고 CGI/bonus route가 아니면 `StaticFileHandler::handlePath()`로 보낸다.

### upload는 어디서 처리하는가?

POST 요청이고 route의 `uploadEnabled`가 true이면 `UploadHandler::handlePost()`가 multipart 또는 raw body를 저장한다.

### DELETE는 어떻게 root 밖 삭제를 막는가?

URI는 먼저 `PathPolicy`와 `Router`를 통해 traversal이 제거/거절된다.  
`DeleteHandler`에서도 relative path의 `..`를 다시 확인한다.

### CGI는 어떻게 non-blocking인가?

`CgiHandler::start()`가 child와 pipe를 만들고, parent 쪽 pipe fd를 non-blocking으로 만든다.  
`EventLoop`가 CGI stdin pipe는 `POLLOUT`, stdout pipe는 `POLLIN`으로 poll에 등록해서 처리한다.

### session bonus는 왜 `/session`에서만 만드는가?

모든 요청마다 session을 만들면 stress test에서 session map이 계속 증가한다.  
그래서 `/session` route일 때만 생성하고, TTL과 최대 개수 제한을 둔다.

---

## 6. 추천 공부 순서

1. `main.cpp`
2. `Config.hpp`, `ConfigParser.hpp`, `ConfigParser.cpp`
3. `Server.hpp`, `Server.cpp`
4. `EventLoop.hpp`, `EventLoop.cpp`
5. `Client.hpp`, `Client.cpp`
6. `HttpRequest.hpp`, `RequestParser.hpp`, `RequestParser.cpp`
7. `Router.hpp`, `Router.cpp`, `PathPolicy.hpp`, `PathPolicy.cpp`
8. `HttpResponse.hpp`, `ResponseBuilder.hpp`
9. `StaticFileHandler`
10. `UploadHandler`
11. `DeleteHandler`
12. `CgiHandler`
13. `tests/`를 보면서 기능별 검증 흐름 확인

이 순서로 보면 “서버가 시작되는 흐름”에서 “요청 하나가 응답 하나로 바뀌는 흐름”까지 자연스럽게 이어진다.
