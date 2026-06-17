# STEP 00 구현 결과

## 목표

`STEP00.md`는 실제 socket, HTTP parser, CGI를 작성하기 전에 팀 전체가 공유할 설계 기준을 고정하는 단계이다. 그래서 이번 작업은 서버를 실행하는 기능이 아니라, 이후 단계에서 공통으로 사용할 정책과 상태를 C++98 코드와 문서로 정리했다.

## 생성한 코드

| 파일 | 역할 |
|---|---|
| `include/webserv/Http.hpp` | Mandatory HTTP method와 status code 정책 선언 |
| `src/http/Http.cpp` | `GET`, `POST`, `DELETE` 판별, `405`와 `501` 구분 기준, reason phrase 제공 |
| `include/webserv/StateMachine.hpp` | Client 상태 머신과 Request Parser 상태 머신 선언 |
| `src/core/StateMachine.cpp` | 각 상태 이름, read/write 가능 상태, terminal parser 상태 판별 |
| `include/webserv/Config.hpp` | config directive 규칙과 `ServerConfig`, `LocationConfig` 구조체 선언 |
| `src/config/Config.cpp` | directive 인자 수, 사용 가능 context, 중복 허용, location 상속 정책 구현 |
| `include/webserv/PathPolicy.hpp` | URI를 파일 경로로 바꾸는 정책 함수 선언 |
| `src/utils/PathPolicy.cpp` | query 제거, percent decode, `.`/`..` 처리, location root 결합 규칙 구현 |
| `config/default.conf` | `STEP00.md` 기준에 맞춘 기본 config 예시 |

## Mandatory 범위

구현 대상으로 고정한 기능은 다음과 같다.

- C++98 기반 HTTP server
- 실행 파일 이름 `webserv`
- config file 기반 server/location 설정
- 여러 listen socket 처리
- 하나의 `poll()` 흐름에서 listen socket, client socket, CGI pipe 관리
- `GET`, `POST`, `DELETE`
- static file serving
- file upload
- DELETE
- CGI
- autoindex
- redirect
- custom error page
- request body size limit
- chunked request body
- client 오류로 서버가 종료되지 않는 error handling

## 이번 단계에서 구현하지 않는 범위

`STEP00.md`의 제한에 맞춰 다음은 아직 구현하지 않았다.

- socket 생성, bind, listen, accept
- `poll()` event loop
- HTTP request parser의 실제 파싱 로직
- response 생성기
- static file, upload, DELETE handler
- CGI fork/execve/pipe 처리
- bonus 기능

## Config 문법 결정

기본 config 예시는 `config/default.conf`에 작성했다.

Directive 정책은 `defaultDirectiveRules()`에 코드로 고정했다.

| Directive | Context | Args | Duplicate | Location 상속 |
|---|---|---:|---|---|
| `server` | top | 0 | yes | no |
| `listen` | server | 1 | yes | no |
| `root` | server/location | 1 | no | yes |
| `index` | server/location | 1-16 | no | yes |
| `client_max_body_size` | server/location | 1 | no | yes |
| `error_page` | server/location | 2 | yes | yes |
| `location` | server | 1 | yes | no |
| `methods` | location | 1-3 | no | no |
| `return` | location | 2 | no | no |
| `autoindex` | location | 1 | no | no |
| `upload` | location | 1 | no | no |
| `upload_store` | location | 1 | no | no |
| `cgi` | location | 2 | yes | no |

잘못된 config가 발견되면 서버 시작 전에 실패시키는 정책으로 진행한다.

## 요청 처리 순서

```text
config 읽기
-> listen socket 생성
-> poll event loop
-> client accept
-> request bytes 누적
-> request 완성 여부 판단
-> request parsing
-> server/location 선택
-> method/body/config 검사
-> static/upload/DELETE/CGI 처리
-> response 생성
-> partial send
-> close 또는 다음 요청
```

초기 연결 정책은 단순성을 위해 `Connection: close`를 기본으로 잡는다. keep-alive는 Mandatory가 안정화된 뒤 확장 대상으로 둔다.

## 상태 머신 결정

Client 상태는 `ClientState` enum으로 고정했다.

```text
READING_HEADERS
READING_BODY
PROCESSING_REQUEST
RUNNING_CGI
WRITING_RESPONSE
CLOSING
```

`READING_HEADERS`, `READING_BODY` 상태에서만 client fd read를 허용한다. `WRITING_RESPONSE` 상태에서만 client fd write를 허용한다.

Request Parser 상태는 `RequestParserState` enum으로 고정했다.

```text
REQUEST_LINE
HEADERS
BODY_BY_LENGTH
BODY_CHUNK_SIZE
BODY_CHUNK_DATA
BODY_CHUNK_CRLF
COMPLETE
ERROR
```

`COMPLETE`, `ERROR`는 parser terminal 상태이다.

## URI와 파일 경로 결합 규칙

`resolveUriPath()` 기준은 다음과 같다.

- query string과 fragment는 파일 경로에서 제외한다.
- request URI는 `/`로 시작해야 한다.
- percent decode를 먼저 수행한다.
- decode 실패, `%00`, 잘못된 escape는 거절한다.
- decode 이후 `..` segment가 있으면 root 밖 접근 시도로 보고 거절한다.
- `.` segment와 중복 slash는 정규화한다.
- location은 longest-prefix match로 선택한다.
- 선택된 location prefix 뒤의 나머지 path를 location root에 붙인다.

예시:

```text
요청 URI: /images/icons/home.png?size=small
location: /images
location root: ./www/images
결과 path: ./www/images/icons/home.png
```

## Status code 정책

| 상황 | Status |
|---|---:|
| 정상 GET | `200 OK` |
| 새 파일 업로드 | `201 Created` |
| DELETE 성공 | `204 No Content` |
| 잘못된 request | `400 Bad Request` |
| 접근 금지 | `403 Forbidden` |
| 리소스 없음 | `404 Not Found` |
| route에서 method 금지 | `405 Method Not Allowed` |
| body 제한 초과 | `413 Payload Too Large` |
| 서버가 구현하지 않는 method | `501 Not Implemented` |
| CGI 실패 | `502 Bad Gateway` |
| CGI timeout | `504 Gateway Timeout` |
| 지원하지 않는 HTTP version | `505 HTTP Version Not Supported` |

`GET`, `POST`, `DELETE`는 서버가 구현하는 method이다. 이 method가 location의 `methods`에 없으면 `405`를 반환한다. 그 외 method는 서버가 구현하지 않는 method로 보고 `501`을 반환한다.

## 담당 모듈 기준

| 모듈 | 담당 |
|---|---|
| `config` | config tokenizer/parser, directive 검증, 상속 처리 |
| `core` | listen socket, event loop, client lifecycle |
| `http` | request/response 타입, parser, routing |
| `handlers` | static file, upload, DELETE, CGI |
| `utils` | path, string, MIME, time, fd helper |

## 완료 체크

- [x] config 예시와 directive 의미가 정해졌다.
- [x] request 처리 흐름을 문서화했다.
- [x] Client와 Parser 상태 머신을 코드로 고정했다.
- [x] URI/path 결합 규칙을 코드로 고정했다.
- [x] 주요 status code 기준을 코드로 고정했다.
- [x] 담당 모듈과 공통 인터페이스 기준을 정했다.
