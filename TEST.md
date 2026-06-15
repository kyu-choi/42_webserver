# Webserv Subject Compliance Checklist

이 문서는 루트의 `en.subject.pdf` Version 24.0을 기준으로 현재 구현 상태를 점검한 결과이다.

상태 표기:

- `[x]` 만족
- `[~]` 기능은 존재하지만 수정 또는 추가 검증 필요
- `[ ]` 미충족
- `[N/A]` 선택 사항이거나 현재 평가 환경에 해당하지 않음

## 현재 판정

Mandatory 기능 대부분과 Bonus 기능은 구현되어 있다. 다만 아래 항목 때문에 현재 상태를
`en.subject.pdf` mandatory 완전 충족으로 판정할 수는 없다.

1. Chunked request의 body 크기 제한이 decoded body가 아니라 raw framing 크기의 영향도 받는다.
2. 기존 파일을 덮어쓰는 업로드도 항상 `201 Created`를 반환한다.
3. 모든 응답에서 세션을 생성하며 세션 만료 정책이 없어 장기 stress test에서 메모리가 계속 증가할 수 있다.
4. 현재 디렉터리가 Git repository가 아니므로 실제 제출 형태를 충족하지 않는다.

## 1. 제출 형식과 빌드

- [x] 실행 파일 이름이 `webserv`이다.
  - 근거: `Makefile`의 `NAME := webserv`
- [x] `Makefile`에 `$(NAME)`, `all`, `clean`, `fclean`, `re` 규칙이 있다.
- [x] 불필요한 relink가 발생하지 않는다.
  - 검증: `make re && make`
  - 결과: 두 번째 `make`에서 `Nothing to be done for 'all'.`
- [x] `c++ -Wall -Wextra -Werror -std=c++98`로 컴파일된다.
- [x] 외부 라이브러리와 Boost를 사용하지 않는다.
- [x] 설정 파일을 선택적 인자로 받는다.
  - `./webserv config/default.conf`
- [x] 인자가 없으면 기본 설정 파일 `config/default.conf`를 사용한다.
- [x] 잘못된 인자 개수와 존재하지 않는 설정 파일을 오류로 처리한다.
- [ ] 현재 디렉터리가 Git repository가 아니다.
  - 확인: `git status`
  - 현재 결과: `fatal: not a git repository`
  - 수정: 실제 제출 전 Git repository를 초기화하거나 42 제출 repository로 파일을 이동한다.

## 2. 허용 함수와 C++98

- [x] 구현은 C++98로 컴파일된다.
- [x] `fork()`는 CGI 실행 경로에서만 사용한다.
  - 근거: `src/Server.cpp`의 CGI 실행 코드
- [x] 다른 웹 서버를 `execve()`하지 않는다.
  - `execve()` 대상은 설정된 Python/PHP CGI interpreter이다.
- [x] 외부 HTTP/config parser 라이브러리를 사용하지 않는다.

- [~] DELETE 구현에서 C++98 표준 라이브러리 함수 `std::remove()`를 사용한다.
  - Subject는 DELETE를 필수로 요구하지만 external function 목록에는 `remove`/`unlink`가 없다.
  - C++98 표준 라이브러리 사용으로 해석하면 허용 가능하지만, 캠퍼스 평가 기준을 사전에 확인하는 것이 안전하다.

## 3. Non-Blocking 이벤트 루프

- [x] 서버는 하나의 `poll()` 호출을 사용하는 이벤트 루프를 가진다.
  - 근거: `src/Server.cpp`의 `Server::run()`
- [x] listen socket도 같은 `poll()`에서 감시한다.
- [x] `poll()`이 읽기와 쓰기를 동시에 감시한다.
  - client socket: `POLLIN` / `POLLOUT`
  - CGI stdin pipe: `POLLOUT`
  - CGI stdout pipe: `POLLIN`
- [x] listen socket, client socket, CGI pipe는 non-blocking으로 설정한다.
- [x] socket `recv()`는 `POLLIN` 이벤트 이후 호출한다.
- [x] socket `send()`는 `POLLOUT` 이벤트 이후 호출한다.
- [x] CGI pipe `read()`는 `POLLIN`/`POLLHUP` 이벤트 이후 호출한다.
- [x] CGI pipe `write()`는 `POLLOUT` 이벤트 이후 호출한다.
- [x] read/write 후 `errno`를 확인해 동작을 조정하지 않는다.
- [x] partial read와 partial write를 client별 buffer와 offset으로 처리한다.
- [x] client disconnect를 처리한다.
- [x] CGI 실행 중 client disconnect를 감지하고 CGI를 종료한다.
- [x] CGI 종료 확인은 `waitpid(..., WNOHANG)`을 사용해 blocking하지 않는다.
- [x] CGI 자식은 상속받은 listen/client/다른 CGI fd를 `execve()` 전에 닫는다.
- [x] 요청 timeout과 CGI timeout이 존재한다.
  - 일반 client timeout: 30초
  - CGI timeout: 5초, `504 Gateway Timeout`

검증:

```bash
rg -n '\bpoll\(' src
rg -n '\bfork\(' src
rg -n 'waitpid' src
python3 tests/cgi_nonblocking.py
```

## 4. HTTP 요청과 응답

- [x] HTTP/1.0과 HTTP/1.1 요청을 파싱한다.
- [x] HTTP/1.1 요청에 `Host`가 없으면 `400 Bad Request`를 반환한다.
- [x] request line, headers, body를 분리해 파싱한다.
- [x] `Content-Length` body를 처리한다.
- [x] `Transfer-Encoding: chunked` 요청을 unchunk한다.
- [x] CGI에는 unchunk된 body를 전달한다.
- [x] 잘못된 request line/header/content length를 거절한다.
- [x] 너무 긴 URI를 `414 URI Too Long`으로 처리한다.
- [x] 지원하지 않는 method를 `501 Not Implemented`로 처리한다.
- [x] HTTP 응답에 status line, `Content-Length`, `Connection`, body를 포함한다.
- [x] partial send를 처리한다.
- [x] 요청이 무기한 기다리지 않도록 timeout을 둔다.

- [~] HTTP status code 대부분은 정확하지만 업로드 overwrite 상태 코드가 부정확하다.
  - 현재 최초 업로드: `201 Created`
  - 현재 동일 파일 덮어쓰기: `201 Created`
  - 수정: 저장 전에 대상 파일 존재 여부를 확인하고 기존 파일 교체 시 `200 OK` 또는 `204 No Content`를 반환한다.
- [~] Chunked body의 최대 크기 검사가 정확하지 않다.
  - 재현: `client_max_body_size 1M`에서 decoded body 200KB, raw framing 약 1.2MB인 요청이 `413`을 반환한다.
  - 원인: `src/Server.cpp`에서 `client.input.size() > maxBodySize + 65536`으로 raw request 크기를 제한한다.
  - 수정: header/framing 제한과 decoded body 제한을 분리한다. Chunk를 읽는 중 decoded body 누적 크기로 `client_max_body_size`를 검사한다.

## 5. Mandatory HTTP 기능

- [x] GET을 지원한다.
  - 정적 파일, index file, autoindex를 제공한다.
- [x] POST를 지원한다.
  - echo, upload, CGI body 전달을 지원한다.
- [x] DELETE를 지원한다.
  - 파일 삭제 성공 시 `204 No Content`
  - 존재하지 않는 파일은 `404 Not Found`
  - 디렉터리 삭제 요청은 `409 Conflict`
- [x] 완전한 정적 웹사이트를 제공한다.
  - `www/index.html`, `www/style.css`, `www/upload.html`
- [x] browser-compatible 응답을 생성한다.
- [x] multipart/form-data 파일 업로드를 지원한다.
- [x] raw body 업로드를 지원한다.
- [x] upload filename의 `/`, `\`, 제어 문자를 거절한다.
- [x] path traversal을 방지한다.
- [x] default error page를 생성한다.
- [x] custom error page를 지원한다.

## 6. 여러 포트와 웹사이트

- [x] 여러 interface:port pair를 설정할 수 있다.
- [x] 여러 포트를 동시에 listen한다.
- [x] 서로 다른 포트에서 서로 다른 콘텐츠를 제공한다.
  - `127.0.0.1:8080` -> `./www`
  - `127.0.0.1:8081` -> `./www/site_b`
- [x] listen socket과 server config를 연결해 요청을 처리한다.

- [N/A] Virtual host는 Subject에서 scope 밖의 선택 기능이다.

## 7. Configuration File

- [x] `server` block을 파싱한다.
- [x] `listen interface:port`를 지원한다.
- [x] `root`를 지원한다.
- [x] `index`를 지원한다.
- [x] `client_max_body_size`를 지원한다.
- [x] `error_page`를 지원한다.
- [x] route별 accepted method 목록을 지원한다.
- [x] HTTP redirect를 지원한다.
- [x] route별 root directory를 지원한다.
- [x] directory listing on/off를 지원한다.
- [x] directory default index file을 지원한다.
- [x] upload 허용 여부와 storage path를 지원한다.
- [x] CGI extension과 interpreter를 지원한다.
- [x] longest-prefix location matching을 사용한다.
- [x] location보다 뒤에 선언된 server root/index도 정상 상속한다.
- [x] 잘못된 설정 값과 알 수 없는 directive를 오류 처리한다.

- [~] `client_max_body_size` 설정은 존재하지만 chunked 요청에서는 정확히 적용되지 않는다.
  - 수정 방법은 HTTP 요청/응답 체크리스트의 chunked body 항목 참고.

## 8. CGI

- [x] 파일 확장자를 기준으로 CGI를 실행한다.
- [x] CGI에서만 `fork()`를 사용한다.
- [x] CGI stdin/stdout pipe를 사용한다.
- [x] CGI pipe는 non-blocking이며 같은 `poll()`에서 감시한다.
- [x] CGI에 request method, URI, query, body, content headers를 전달한다.
- [x] 일반 요청 header를 `HTTP_*` 환경변수로 전달한다.
- [x] CGI에 unchunked body를 전달하고 stdin EOF를 보낸다.
- [x] CGI output에 Content-Length가 없어도 stdout EOF로 종료를 판단한다.
- [x] CGI script directory로 `chdir()` 후 실행한다.
- [x] CGI timeout을 처리한다.
- [x] 잘못된 CGI interpreter/비정상 종료를 `502 Bad Gateway`로 처리한다.
- [x] Python CGI를 지원한다.
- [x] PHP CGI를 지원한다.

## 9. 안정성과 Stress Test

- [x] malformed HTTP 요청 후에도 서버가 계속 동작한다.
- [x] 느린 client 하나가 다른 client 처리를 막지 않는다.
- [x] CGI 실행 중에도 다른 요청을 처리한다.
- [x] CGI timeout/disconnect 후 자식 프로세스를 회수한다.
- [x] 300개 concurrent request 자동 테스트를 통과한다.
- [x] Valgrind 핵심 경로에서 오류와 leak이 없다.
  - 결과: `0 errors`, `0 bytes in 0 blocks`

- [~] Bonus session 저장소가 무제한 증가한다.
  - 현재 모든 응답에서 새 세션을 만들고 `_sessions`에서 제거하지 않는다.
  - 쿠키 없는 요청 10,000개에서 RSS가 약 920KB 증가했다.
  - 수정: `/session`처럼 세션이 필요한 route에서만 세션을 생성하고, 마지막 접근 시간/TTL/최대 개수 기반으로 만료 처리한다.
  - Mandatory와 직접 연결된 기능은 아니지만 `remain operational at all times` 조건과 Bonus 완성도에 위험하다.

## 10. README 요구사항

- [x] 루트에 영어 `README.md`가 있다.
- [x] 첫 줄이 italic 형식이며 42 curriculum과 login을 표시한다.
- [x] `Description` section이 있다.
- [x] `Instructions` section이 있다.
- [x] `Resources` section이 있다.
- [x] AI 사용 목적과 구현 영역을 설명한다.
- [ ] 현재 위치가 Git repository가 아니므로 "Git repository root의 README" 조건은 아직 충족하지 않는다.

## 11. Bonus

- [~] Cookie와 session management 예제가 있다.
  - `/session`과 `WSID` cookie가 동작한다.
  - 세션 만료/정리 정책이 없어 수정 권장.

- [x] Multiple CGI types를 지원한다.
  - Python `.py`
  - PHP `.php`

- [~] Bonus는 mandatory가 완전히 통과해야 평가된다.
  - Chunked body size와 upload overwrite status를 먼저 수정해야 한다.

## 12. 제공된 테스트 파일

- [x] 설정 파일: `config/default.conf`, `config/minimal.conf`
- [x] 정적 사이트와 custom error page
- [x] 업로드/DELETE 시연 route
- [x] Python/PHP CGI 시연 파일
- [x] cookie/session 시연 route
- [x] shell + curl 통합 테스트
- [x] Python chunked request 테스트
- [x] Python CGI non-blocking 테스트
- [x] Python concurrent stress 테스트

- [~] NGINX 비교 테스트는 문서에서 권장하지만 자동화되어 있지 않다.
  - Subject에서 필수는 아니지만 평가 전 주요 응답/status/header를 NGINX와 비교하는 것이 좋다.

## 13. 실행할 테스트 명령

### 전체 빌드 및 자동 테스트

```bash
make re
make
./tests/integration.sh
```

기대 결과:

```text
make: Nothing to be done for 'all'.
PASS chunked request
PASS CGI non-blocking test
PASS stress test: 300 requests
PASS integration suite
```

### Valgrind

```bash
valgrind --leak-check=full --show-leak-kinds=all --track-fds=yes \
  ./webserv config/default.conf
```

다른 터미널에서 GET, upload, DELETE, CGI, timeout 요청을 보낸 뒤 `Ctrl-C`로 종료한다.

### 수동 기능 테스트

```bash
./webserv config/default.conf

curl -v http://127.0.0.1:8080/
curl -v http://127.0.0.1:8081/
curl -v http://127.0.0.1:8080/not-found
curl -v -X DELETE http://127.0.0.1:8080/
curl -v http://127.0.0.1:8080/old
curl -v -X POST --data-binary 'hello' http://127.0.0.1:8080/echo
curl -v -F file=@README.md http://127.0.0.1:8080/upload
curl -v http://127.0.0.1:8080/uploads/
curl -v -X DELETE http://127.0.0.1:8080/delete/README.md
curl -v 'http://127.0.0.1:8080/cgi-bin/hello.py?name=test'
curl -v 'http://127.0.0.1:8080/cgi-bin/hello.php?name=test'
curl -c /tmp/webserv-cookie http://127.0.0.1:8080/session
curl -b /tmp/webserv-cookie http://127.0.0.1:8080/session
```

## 14. 수정 우선순위

1. **Mandatory:** Chunked request의 decoded body 기준 `client_max_body_size` 검사
2. **Mandatory:** 기존 파일 overwrite 시 정확한 업로드 status code 반환
3. **Submission:** 실제 Git repository 구성 확인
4. **Resilience/Bonus:** 세션을 필요한 route에서만 만들고 TTL/개수 제한 추가
5. **평가 준비:** NGINX/telnet 비교 결과와 설명 준비

