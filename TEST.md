# Webserv 평가 체크리스트

이 문서는 현재 구현 상태와 STEP18/STEP19 검증 환경을 기준으로 정리한 한글 평가 체크리스트이다.

상태 표기:

- `[x]` 구현되어 있고 테스트로 확인됨
- `[~]` 구현되어 있으나 평가 환경에서 한 번 더 확인 권장
- `[ ]` 아직 구현되지 않음
- `[N/A]` 현재 mandatory 범위 밖

## 제출 형식과 빌드

- [x] 실행 파일 이름은 `webserv`이다.
- [x] `Makefile`에 `all`, `clean`, `fclean`, `re` 규칙이 있다.
- [x] 컴파일 플래그에 `-Wall -Wextra -Werror -std=c++98`이 포함되어 있다.
- [x] 깨끗하게 빌드한 뒤 다시 `make`를 실행해도 불필요한 relink가 발생하지 않는다.
- [x] 실행 시 config 파일 인자를 0개 또는 1개 받을 수 있다.
- [x] 인자가 없으면 `config/default.conf`를 사용한다.
- [x] `README.md`는 repository root에 있다.
- [x] `README.md`는 영어 공식 README이며 subject가 요구하는 첫 줄 이탤릭 문구와 Description/Instructions/Resources(AI 사용 방식 포함) 섹션을 갖춘다.
- [x] 한국어 참고 문서는 `README_KO.md`로 유지한다.
- [~] 최종 제출 전 현재 디렉토리가 실제 제출할 Git repository root인지 확인한다.

## Mandatory 런타임 구조

- [x] 하나의 `poll()` 기반 event loop를 사용한다.
- [x] listen socket은 non-blocking이다.
- [x] client socket은 non-blocking이다.
- [x] CGI stdin/stdout pipe는 non-blocking이다.
- [x] poll readiness 확인 없이 blocking read/write를 수행하지 않는다.
- [x] partial recv와 partial send를 client별 buffer로 처리한다.
- [x] client timeout이 있다.
- [x] `SIGINT`, `SIGTERM`으로 graceful shutdown을 처리한다.
- [x] `SIGPIPE`를 무시한다.
- [x] 동시에 받을 client 수에 상한이 있다.
- [x] CGI child process는 `waitpid(..., WNOHANG)`으로 회수한다.

## HTTP 동작

- [x] HTTP/1.0과 HTTP/1.1 request line을 파싱한다.
- [x] HTTP/1.1 요청에 `Host` header가 없으면 `400 Bad Request`를 반환한다.
- [x] 잘못된 request line, header, body framing을 거절한다.
- [x] `Content-Length` body를 파싱한다.
- [x] `Transfer-Encoding: chunked` body를 파싱한다.
- [x] chunked body는 handler와 CGI에 전달되기 전에 decoded body로 변환된다.
- [x] `client_max_body_size`는 effective decoded request body 기준으로 검사한다.
- [x] 구현하지 않은 method는 `501 Not Implemented`를 반환한다.
- [x] route config에서 허용하지 않은 method는 `405 Method Not Allowed`를 반환한다.
- [x] `405` 응답에는 `Allow` header가 포함된다.
- [x] 응답에는 `Content-Length`가 포함된다.
- [x] 설정된 custom error page가 있으면 해당 error page를 사용한다.

## Config

- [x] `server` block을 파싱한다.
- [x] `listen host:port`를 파싱한다.
- [x] `root`를 server/location level에서 지원한다.
- [x] `index`를 server/location level에서 지원한다.
- [x] `client_max_body_size`를 지원한다.
- [x] `error_page`를 지원한다.
- [x] `location`을 지원한다.
- [x] `methods`를 지원한다.
- [x] `return 301|302 target` redirect를 지원한다.
- [x] `autoindex on|off`를 지원한다.
- [x] `upload on|off`와 `upload_store`를 지원한다.
- [x] `cgi extension interpreter`를 지원한다.
- [x] server level 설정이 필요한 location으로 상속된다.
- [x] routing은 longest-prefix location matching을 사용한다.
- [x] 잘못된 config는 server loop를 열기 전에 실패한다.

## 정적 파일과 라우팅

- [x] `www/` 아래의 정적 HTML을 제공한다.
- [x] CSS와 SVG 파일을 적절한 MIME type으로 제공한다.
- [x] 존재하지 않는 파일은 `404`를 반환한다.
- [x] path traversal 요청을 거절한다.
- [x] directory 요청의 trailing slash redirect를 처리한다.
- [x] directory index 우선순위를 처리한다.
- [x] location별로 autoindex를 켤 수 있다.
- [x] location별로 autoindex를 끌 수 있다.
- [x] `/old` redirect route가 있다.
- [x] `8080`, `8081` multi-port demo에서 서로 다른 content를 제공한다.

## Upload와 DELETE

- [x] `/upload`에서 multipart upload를 지원한다.
- [x] raw body upload를 지원한다.
- [x] upload directory가 쓰기 가능한지 확인한다.
- [x] upload filename에서 `/`, `\`, 제어 문자, `.`, `..`를 거절한다.
- [x] binary upload content를 보존한다.
- [x] 새 파일 upload는 `201 Created`를 반환한다.
- [x] 기존 파일 overwrite는 `200 OK`를 반환한다.
- [x] DELETE route를 통해 파일을 삭제한다.
- [x] 없는 파일 DELETE는 `404`를 반환한다.
- [x] directory DELETE는 거절한다.
- [x] DELETE traversal은 거절한다.

## CGI

- [x] config의 extension mapping으로 CGI 여부를 판단한다.
- [x] `fork()`는 CGI 실행 경로에서만 사용한다.
- [x] CGI stdin/stdout은 pipe로 연결한다.
- [x] CGI pipe도 같은 event loop에서 처리한다.
- [x] CGI에 request method, URI, query string, content header, 일반 request header를 환경변수로 전달한다.
- [x] CGI stdin에는 decoded request body를 전달한다.
- [x] CGI output을 HTTP response로 변환한다.
- [x] CGI timeout은 `504 Gateway Timeout`을 반환한다.
- [x] 잘못된 CGI output 또는 실행 실패는 `502 Bad Gateway`를 반환한다.
- [x] 느린 CGI가 정적 요청 처리를 막지 않는다.

## 실행할 테스트 명령

평가 전 권장 실행:

```sh
make fclean
make
make
./tests/integration.sh
```

빠르게 확인할 때:

```sh
SKIP_VALGRIND=1 ./tests/integration.sh
```

평가 시연용 config:

```sh
./webserv config/step19.conf
```

수동 확인용 curl:

```sh
curl -i http://127.0.0.1:8080/
curl -i http://127.0.0.1:8081/
curl -i http://127.0.0.1:8080/no-such-file
curl -i http://127.0.0.1:8080/old
curl -i http://127.0.0.1:8080/post-only
curl -i http://127.0.0.1:8080/listing/
curl -i "http://127.0.0.1:8080/cgi-bin/hello.py?name=eval"
```

## 평가 중 설명할 코드 위치

- `src/main.cpp`: 인자 처리, signal 설정, config parsing, server 시작
- `src/config/ConfigParser.cpp`: tokenizer, directive 검증, server/location config 생성
- `src/core/Server.cpp`: socket 생성, bind/listen, non-blocking 설정
- `src/core/EventLoop.cpp`: `poll()` loop, accept, recv, send, timeout, CGI fd 처리, cleanup
- `src/http/RequestParser.cpp`: request line, headers, content length, chunked body, parser error state
- `src/http/Router.cpp`: effective server/location config, longest-prefix matching
- `src/handlers/StaticFileHandler.cpp`: file, directory, index, autoindex 처리
- `src/handlers/UploadHandler.cpp`: multipart/raw upload parsing과 저장
- `src/handlers/DeleteHandler.cpp`: 안전한 파일 삭제
- `src/handlers/CgiHandler.cpp`: CGI environment, pipe, fork/exec 처리
- `src/http/ResponseBuilder.cpp`: HTTP response 생성

## 짧은 수정 연습

평가 전 다음 변경을 직접 연습하면 좋다.

- `src/utils/MimeTypes.cpp`에 MIME type 하나 추가하기
- `src/core/EventLoop.cpp`에서 client timeout 값 변경하기
- `src/core/EventLoop.cpp`에서 CGI timeout 값 변경하기
- `www/errors/`에 custom error page 추가하고 config에 연결하기
- `config/step19.conf`에 새 static location 추가하기

## Bonus 상태

- [x] cookie parsing을 구현했다.
- [x] `/session` route에서만 session을 생성한다.
- [x] session id를 `WSID` cookie로 전달한다.
- [x] session별 방문 횟수와 마지막 접근 시간을 저장한다.
- [x] session TTL을 적용한다.
- [x] session 최대 개수 제한을 적용한다.
- [x] 일반 정적 요청에서는 session을 만들지 않는다.
- [x] 여러 CGI interpreter type을 같은 non-blocking CGI pipe 구조로 처리한다.
- [x] `config/step20.conf`에서 `.py`와 `.sh` CGI type을 시연한다.
- [x] `tests/bonus.py`에서 cookie/session과 여러 CGI type을 자동 검증한다.
