# Webserv 구현 단계별 가이드

## 0. 이 문서의 목적

이 문서는 현재 디렉토리에 있는 모든 자료를 바탕으로, 사용자가 **C++98 HTTP 서버 `webserv`를 처음부터 구현할 때 해야 할 일**을 실제 작업 순서대로 정리한 체크리스트이다.

참고한 파일:

- `en.subject.pdf`: Version 24.0 공식 과제 명세. 최종 판단 기준이다.
- `GUIDE.md`: 전체 요구사항, 추천 클래스 구조, 구현 순서, 테스트 방법.
- `SETTING.md`: 설정 파일 문법, 상속, route 처리 우선순위.
- `STRUCTURE.md`: 소켓, 이벤트 루프, client buffer, 상태 머신 설계.
- `README.md`, `README_KR.md`: 목표 기능, 실행법, 저장소 구조, 평가 설명 항목.
- `TEST.md`: 기능별 검증 항목과 알려진 위험 요소.
- `NGINX.md`: NGINX와 webserv의 역할 비교.
- `html.md`: 브라우저, TCP, HTTP 요청/응답의 기본 흐름.

문서 간 내용이 충돌하거나 범위가 다를 때는 다음 우선순위를 따른다.

```text
en.subject.pdf 공식 명세
→ 실제 평가 환경과 평가표
→ 구현 및 테스트로 확인한 동작
→ 나머지 참고 문서
```

---

## 바로 따라 하는 Step-by-Step 실행 계획

이 절은 실제 구현할 때 위에서 아래로 순서대로 진행하기 위한 작업 목록이다.

진행 규칙:

```text
1. 현재 Step의 체크박스를 순서대로 처리한다.
2. 각 Step의 테스트를 실행한다.
3. 완료 기준을 모두 만족한 경우에만 다음 Step으로 이동한다.
4. 실패하면 다음 기능을 추가하지 말고 현재 Step부터 수정한다.
5. 구현 중 세부 내용이 필요하면 문서 뒤쪽의 관련 상세 설명을 참고한다.
```

현재 디렉토리에는 구현 코드가 없으므로 **Step 00부터 시작**한다.

### 전체 진행 순서

각 Step의 독립 상세 문서를 열고 체크박스를 위에서 아래로 진행한다.

| Step | 상세 문서 | 작업 |
|---:|---|---|
| 00 | [STEP00.md](STEP00.md) | 요구사항과 설계 결정 |
| 01 | [STEP01.md](STEP01.md) | 프로젝트 폴더와 Makefile |
| 02 | [STEP02.md](STEP02.md) | 최소 TCP 서버 |
| 03 | [STEP03.md](STEP03.md) | poll 기반 이벤트 루프 |
| 04 | [STEP04.md](STEP04.md) | Client 상태와 partial I/O |
| 05 | [STEP05.md](STEP05.md) | HTTP Request Parser |
| 06 | [STEP06.md](STEP06.md) | HTTP Response와 기본 에러 |
| 07 | [STEP07.md](STEP07.md) | 정적 파일 GET |
| 08 | [STEP08.md](STEP08.md) | Config Parser |
| 09 | [STEP09.md](STEP09.md) | 여러 포트와 Location Routing |
| 10 | [STEP10.md](STEP10.md) | Method 제한, Redirect, Custom Error |
| 11 | [STEP11.md](STEP11.md) | Directory Index와 Autoindex |
| 12 | [STEP12.md](STEP12.md) | Request Body와 크기 제한 |
| 13 | [STEP13.md](STEP13.md) | POST 파일 업로드 |
| 14 | [STEP14.md](STEP14.md) | DELETE |
| 15 | [STEP15.md](STEP15.md) | Chunked Request |
| 16 | [STEP16.md](STEP16.md) | CGI |
| 17 | [STEP17.md](STEP17.md) | Timeout과 자원 정리 |
| 18 | [STEP18.md](STEP18.md) | 통합 테스트와 Stress Test |
| 19 | [STEP19.md](STEP19.md) | README와 평가 준비 |
| 20 | [STEP20.md](STEP20.md) | Mandatory 완성 후 Bonus |

---

### Step 00. 요구사항과 설계 결정

목표: 코드를 작성하기 전에 팀 전체가 같은 규칙으로 구현하도록 기준을 고정한다.

해야 할 일:

- [ ] `en.subject.pdf`의 Mandatory 요구사항을 읽는다.
- [ ] C++98과 허용 함수 목록을 확인한다.
- [ ] 이벤트 감시 방식으로 `poll()`을 사용할지 결정한다.
- [ ] config 문법과 directive 이름을 결정한다.
- [ ] location root와 URI를 실제 파일 경로로 바꾸는 규칙을 결정한다.
- [ ] 초기 연결 정책을 `Connection: close`로 할지 keep-alive까지 구현할지 결정한다.
- [ ] 각 HTTP 상황에 반환할 status code 기준을 정한다.
- [ ] 클래스와 파일의 담당자를 나눈다.

반드시 기록할 설계:

```text
Config 구조
Client 상태 머신
Request Parser 상태 머신
listen fd / client fd / CGI fd 구분 방법
location longest-prefix match 규칙
URI와 filesystem path 결합 규칙
timeout 기준
```

완료 기준:

- [ ] 팀원 모두가 요청 하나의 전체 처리 흐름을 설명할 수 있다.
- [ ] `poll()` 밖에서 socket/pipe I/O를 하지 않는다는 규칙에 동의했다.
- [ ] config 예시 하나를 작성할 수 있을 정도로 문법이 정해졌다.

---

### Step 01. 프로젝트 폴더와 Makefile 만들기

목표: 아무 기능이 없어도 C++98 프로젝트가 안정적으로 빌드되게 만든다.

먼저 만들 것:

```text
Makefile
include/
src/
src/main.cpp
src/config/
src/core/
src/http/
src/handlers/
src/utils/
config/
www/
tests/
```

구현 순서:

- [ ] `src/main.cpp`에 최소 `main()`을 작성한다.
- [ ] 실행 인자는 `0개 또는 config 경로 1개`만 허용하도록 검사한다.
- [ ] 인자가 없을 때 사용할 기본 config 경로를 정한다.
- [ ] `Makefile`의 `NAME`을 `webserv`로 설정한다.
- [ ] `all`, `clean`, `fclean`, `re`, `$(NAME)` 규칙을 작성한다.
- [ ] `-Wall -Wextra -Werror -std=c++98` 플래그를 적용한다.
- [ ] object 파일용 디렉토리와 header dependency 규칙을 만든다.

실행 테스트:

```bash
make
make
make clean
make re
./webserv
./webserv config/default.conf
./webserv a.conf b.conf
```

완료 기준:

- [ ] 첫 번째 `make`가 성공한다.
- [ ] 두 번째 `make`에서 불필요한 relink가 발생하지 않는다.
- [ ] `make clean`, `make fclean`, `make re`가 정상 동작한다.
- [ ] 잘못된 인자 개수에서 명확한 오류를 출력하고 종료한다.

---

### Step 02. 최소 TCP 서버 만들기

목표: `127.0.0.1:8080`에 접속하면 고정된 HTTP 응답을 받는다.

먼저 만들 파일 예시:

```text
include/Server.hpp
src/core/Server.cpp
```

구현 순서:

- [ ] `socket()`으로 listen socket을 만든다.
- [ ] `setsockopt()`로 주소 재사용 option을 설정한다.
- [ ] `bind()`로 `127.0.0.1:8080`에 연결한다.
- [ ] `listen()`으로 접속 대기 상태로 만든다.
- [ ] `accept()`로 client 연결을 받는다.
- [ ] 고정된 HTTP response를 전송한다.
- [ ] client fd와 listen fd를 정상적으로 닫는다.

고정 응답 예시:

```http
HTTP/1.1 200 OK\r
Content-Type: text/plain\r
Content-Length: 12\r
Connection: close\r
\r
Hello World!
```

실행 테스트:

```bash
./webserv
curl -i http://127.0.0.1:8080/
printf 'GET / HTTP/1.1\r\nHost: localhost\r\n\r\n' | nc 127.0.0.1 8080
```

완료 기준:

- [ ] curl과 브라우저에서 `Hello World!`가 보인다.
- [ ] status line과 header 형식이 올바르다.
- [ ] `Content-Length`와 실제 body 크기가 같다.

주의: 이 단계의 blocking 구조는 확인용이다. 다음 Step에서 반드시 poll 기반 구조로 교체한다.

---

### Step 03. poll 기반 이벤트 루프 만들기

목표: listen socket과 여러 client socket을 하나의 `poll()` 흐름에서 처리한다.

먼저 만들 파일 예시:

```text
include/EventLoop.hpp
src/core/EventLoop.cpp
```

구현 순서:

- [ ] listen fd를 non-blocking으로 설정한다.
- [ ] listen fd를 poll 목록에 `POLLIN`으로 등록한다.
- [ ] listen fd에 `POLLIN`이 발생했을 때만 `accept()`한다.
- [ ] 새 client fd를 non-blocking으로 설정한다.
- [ ] client fd를 poll 목록에 등록한다.
- [ ] client fd의 `POLLIN`에서만 `recv()`한다.
- [ ] 보낼 응답이 있을 때만 `POLLOUT`을 등록한다.
- [ ] client fd의 `POLLOUT`에서만 `send()`한다.
- [ ] `POLLERR`, `POLLHUP`, `POLLNVAL`을 처리한다.
- [ ] fd를 닫을 때 poll 목록에서도 제거한다.

실행 테스트:

```bash
curl http://127.0.0.1:8080/ &
curl http://127.0.0.1:8080/ &
curl http://127.0.0.1:8080/ &
```

추가 확인:

```text
nc로 연결만 열고 요청을 보내지 않는다.
그 상태에서 다른 터미널의 curl 요청이 정상 처리되는지 확인한다.
```

완료 기준:

- [ ] 여러 client가 동시에 응답을 받는다.
- [ ] 느린 client 하나가 다른 client를 막지 않는다.
- [ ] client가 연결을 끊어도 서버가 종료되지 않는다.
- [ ] idle 상태에서 CPU가 계속 100%를 사용하지 않는다.

---

### Step 04. Client 상태와 partial I/O 구현

목표: 요청과 응답이 여러 조각으로 나뉘어도 정상 처리한다.

먼저 만들 파일 예시:

```text
include/Client.hpp
src/core/Client.cpp
```

Client에 저장할 값:

```text
client fd
input buffer
output buffer
send offset
현재 상태
마지막 활동 시간
들어온 listen fd
```

구현 순서:

- [ ] client fd별 `Client` 객체를 저장한다.
- [ ] `recv()`한 byte 수만큼 input buffer에 append한다.
- [ ] response 전체를 한 번에 보낼 수 있다고 가정하지 않는다.
- [ ] 실제 `send()` 성공 byte만큼 send offset을 증가시킨다.
- [ ] 전송할 데이터가 남으면 다음 `POLLOUT`을 기다린다.
- [ ] response 전송이 끝나면 client를 닫는다.
- [ ] 모든 close 경로를 공통 cleanup 함수로 모은다.

실행 테스트:

- [ ] Python 또는 `nc`로 요청을 한 글자씩 보낸다.
- [ ] 큰 body를 가진 응답을 보낸다.
- [ ] 응답을 읽지 않는 느린 client를 연결한다.
- [ ] 전송 중 client 연결을 끊는다.

완료 기준:

- [ ] partial recv와 partial send가 정상 동작한다.
- [ ] 연결 종료 후 poll 목록과 client map에 fd가 남지 않는다.
- [ ] 한 client 오류가 다른 client에 영향을 주지 않는다.

---

### Step 05. HTTP Request Parser 구현

목표: raw request에서 method, URI, version, headers, body 정보를 추출한다.

먼저 만들 파일 예시:

```text
include/HttpRequest.hpp
include/RequestParser.hpp
src/http/HttpRequest.cpp
src/http/RequestParser.cpp
```

구현 순서:

- [ ] `\r\n\r\n`으로 header 끝을 찾는다.
- [ ] request line을 method, target, version으로 나눈다.
- [ ] target을 path와 query string으로 나눈다.
- [ ] header line을 key와 value로 파싱한다.
- [ ] HTTP/1.1 요청에 `Host`가 있는지 검사한다.
- [ ] `Content-Length`를 안전하게 숫자로 변환한다.
- [ ] header 이후 body가 완성되었는지 판단한다.
- [ ] request line, header, URI 최대 크기를 정한다.
- [ ] malformed request의 error status를 저장한다.
- [ ] 첫 요청 뒤 남은 input bytes를 보존한다.

이 단계에서는 먼저 `Content-Length` body까지만 처리하고 chunked는 Step 15에서 추가한다.

실행 테스트:

```bash
printf 'GET /hello?name=kyu HTTP/1.1\r\nHost: localhost\r\nX-Test: yes\r\n\r\n' \
  | nc 127.0.0.1 8080
```

추가 malformed 테스트:

```text
request line token 부족
colon 없는 header
HTTP/1.1 Host 누락
숫자가 아닌 Content-Length
지원하지 않는 HTTP version
```

완료 기준:

- [ ] 여러 recv로 나뉜 request를 완성 후 파싱한다.
- [ ] body가 덜 들어오면 기다린다.
- [ ] 잘못된 request가 서버를 죽이지 않는다.
- [ ] parser 결과를 debug 출력하여 정확히 확인할 수 있다.

---

### Step 06. HTTP Response와 기본 에러 구현

목표: 모든 handler가 공통 방식으로 정상 응답과 에러 응답을 만든다.

먼저 만들 파일 예시:

```text
include/HttpResponse.hpp
include/ResponseBuilder.hpp
include/ErrorPageHandler.hpp
src/http/HttpResponse.cpp
src/http/ResponseBuilder.cpp
src/handlers/ErrorPageHandler.cpp
```

구현 순서:

- [ ] status code와 reason phrase 표를 만든다.
- [ ] response의 status, headers, body 구조를 만든다.
- [ ] response를 raw HTTP 문자열로 직렬화한다.
- [ ] body 크기로 `Content-Length`를 계산한다.
- [ ] 기본 `Content-Type`과 `Connection`을 설정한다.
- [ ] 기본 HTML error page 생성 함수를 만든다.
- [ ] 최소 `400`, `403`, `404`, `405`, `413`, `500`, `501`, `505`를 처리한다.

실행 테스트:

```text
정상 요청 → 200
잘못된 request 문법 → 400
없는 route → 404
지원하지 않는 method → 501
지원하지 않는 version → 505
```

완료 기준:

- [ ] browser가 응답 후 무한 로딩하지 않는다.
- [ ] 모든 응답의 `Content-Length`가 정확하다.
- [ ] 에러가 발생해도 다음 요청을 정상 처리한다.

---

### Step 07. 정적 파일 GET 구현

목표: 실제 HTML, CSS, 이미지 파일을 브라우저에 제공한다.

먼저 만들 파일 예시:

```text
include/StaticFileHandler.hpp
include/MimeTypes.hpp
src/handlers/StaticFileHandler.cpp
src/utils/MimeTypes.cpp
www/index.html
www/style.css
```

구현 순서:

- [ ] 초기에는 root를 `./www`로 고정한다.
- [ ] URI에서 query string을 제외한다.
- [ ] URI를 실제 파일 경로로 변환한다.
- [ ] `stat()`으로 파일 존재 여부와 종류를 확인한다.
- [ ] regular file 내용을 binary-safe하게 읽는다.
- [ ] 확장자에 맞는 MIME type을 지정한다.
- [ ] 존재하지 않는 파일은 `404`로 처리한다.
- [ ] 접근할 수 없는 파일은 `403`으로 처리한다.

실행 테스트:

```bash
curl -i http://127.0.0.1:8080/
curl -i http://127.0.0.1:8080/index.html
curl -i http://127.0.0.1:8080/style.css
curl -i http://127.0.0.1:8080/not-found
```

완료 기준:

- [ ] 브라우저에서 HTML과 CSS가 정상 표시된다.
- [ ] 이미지와 binary 파일이 손상되지 않는다.
- [ ] 큰 파일도 partial send로 끝까지 전달된다.

---

### Step 08. Config Parser 구현

목표: 하드코딩된 port와 root를 config 파일 설정으로 교체한다.

먼저 만들 파일 예시:

```text
include/ConfigTypes.hpp
include/ConfigParser.hpp
src/config/ConfigParser.cpp
config/default.conf
config/invalid/
```

구현 순서:

- [ ] `{`, `}`, `;`, 일반 문자열 token을 구분한다.
- [ ] `server` block을 파싱한다.
- [ ] `location` block을 파싱한다.
- [ ] directive별 인자 개수를 검증한다.
- [ ] 알 수 없는 directive를 오류 처리한다.
- [ ] 중괄호와 semicolon 오류를 처리한다.
- [ ] default 값과 상속 규칙을 정한다.
- [ ] 모든 config를 검증한 뒤 socket을 생성한다.

최소 directive:

```text
listen
root
index
client_max_body_size
error_page
location
methods
return
autoindex
upload
upload_store
cgi
```

실행 테스트:

```bash
./webserv config/default.conf
./webserv config/invalid/missing_semicolon.conf
./webserv config/invalid/bad_port.conf
./webserv config/invalid/unknown_directive.conf
```

완료 기준:

- [ ] 정상 config가 내부 `ServerConfig`와 `LocationConfig`로 변환된다.
- [ ] 잘못된 config에서는 listen 전에 명확한 오류로 종료한다.
- [ ] config 값으로 port와 root가 실제로 변경된다.

---

### Step 09. 여러 포트와 Location Routing 구현

목표: 하나의 프로세스가 여러 포트와 여러 route를 처리한다.

먼저 만들 파일 예시:

```text
include/Router.hpp
src/http/Router.cpp
www/site_b/
```

구현 순서:

- [ ] config의 모든 interface:port에 listen socket을 만든다.
- [ ] listen fd와 server config의 관계를 저장한다.
- [ ] 모든 listen fd를 같은 poll loop에 등록한다.
- [ ] client가 들어온 listen fd를 기록한다.
- [ ] URI와 일치하는 모든 location을 찾는다.
- [ ] 가장 긴 prefix의 location을 선택한다.
- [ ] server 기본값과 location override를 합친다.
- [ ] 선택된 location root를 기준으로 실제 path를 만든다.

실행 테스트:

```bash
curl -i http://127.0.0.1:8080/
curl -i http://127.0.0.1:8081/
curl -i http://127.0.0.1:8080/images/a.png
curl -i http://127.0.0.1:8080/images/icons/a.png
```

완료 기준:

- [ ] 8080과 8081이 서로 다른 콘텐츠를 제공한다.
- [ ] `/`, `/images`, `/images/icons` 중 가장 긴 prefix가 선택된다.
- [ ] location에 값이 없으면 server 값을 상속한다.

---

### Step 10. Method 제한, Redirect, Custom Error 구현

목표: location 설정에 따라 요청을 파일 처리 전에 분기한다.

구현 순서:

- [ ] redirect 설정이 있으면 즉시 redirect response를 만든다.
- [ ] location별 허용 method를 검사한다.
- [ ] 금지된 method는 `405 Method Not Allowed`로 처리한다.
- [ ] `405` 응답에 `Allow` header를 추가한다.
- [ ] 서버가 구현하지 않은 method는 `501`로 처리한다.
- [ ] custom error page 설정을 적용한다.
- [ ] custom error file을 읽지 못하면 기본 error page를 사용한다.

처리 순서:

```text
server 선택
→ location 선택
→ redirect
→ method 검사
→ body 크기 검사
→ 실제 handler
```

실행 테스트:

```bash
curl -i http://127.0.0.1:8080/old
curl -i -X DELETE http://127.0.0.1:8080/
curl -i -X PUT http://127.0.0.1:8080/
curl -i http://127.0.0.1:8080/not-found
```

완료 기준:

- [ ] redirect에 올바른 status와 `Location` header가 있다.
- [ ] route별 method 제한이 적용된다.
- [ ] custom error page가 표시된다.

---

### Step 11. Directory Index와 Autoindex 구현

목표: directory 요청을 설정에 따라 index 또는 파일 목록으로 처리한다.

구현 순서:

- [ ] 실제 path가 directory인지 확인한다.
- [ ] 설정된 index 파일을 순서대로 찾는다.
- [ ] index가 있으면 해당 파일을 응답한다.
- [ ] index가 없고 autoindex가 켜져 있으면 목록 HTML을 만든다.
- [ ] index가 없고 autoindex가 꺼져 있으면 `403`을 반환한다.
- [ ] autoindex의 파일명과 link를 HTML escape한다.
- [ ] parent/current directory link 정책을 정한다.

실행 테스트:

```bash
curl -i http://127.0.0.1:8080/
curl -i http://127.0.0.1:8080/listing/
curl -i http://127.0.0.1:8080/private-directory/
```

완료 기준:

- [ ] index 파일이 autoindex보다 우선한다.
- [ ] autoindex link로 실제 파일과 directory를 열 수 있다.
- [ ] autoindex off인 directory는 `403`이다.

---

### Step 12. Request Body와 크기 제한 구현

목표: `Content-Length` body를 완전히 받은 뒤 처리하고 크기 제한을 적용한다.

구현 순서:

- [ ] header에서 `Content-Length`를 확인한다.
- [ ] config의 `client_max_body_size`와 비교한다.
- [ ] 제한 초과면 body 전체를 받기 전에 `413`을 만든다.
- [ ] 제한 이하이면 body가 모두 올 때까지 client buffer에 누적한다.
- [ ] 실제 body 누적 크기도 계속 검사한다.
- [ ] body가 완성된 뒤에만 handler를 호출한다.
- [ ] header limit과 body limit을 별도로 관리한다.

실행 테스트:

```bash
curl -i -X POST --data-binary 'hello' http://127.0.0.1:8080/echo
```

추가 테스트:

```text
Content-Length보다 body가 짧음
Content-Length보다 body가 큼
body limit 정확히 일치
body limit 1 byte 초과
body를 한 byte씩 천천히 전송
```

완료 기준:

- [ ] body가 완성되기 전에 요청을 처리하지 않는다.
- [ ] 제한 초과 요청이 `413`이다.
- [ ] 느린 body 전송 중에도 다른 client를 처리한다.

---

### Step 13. POST 파일 업로드 구현

목표: 업로드 route에서 raw body 또는 multipart 파일을 안전하게 저장한다.

먼저 만들 파일 예시:

```text
include/UploadHandler.hpp
src/handlers/UploadHandler.cpp
www/upload.html
www/uploads/
```

구현 순서:

- [ ] route에서 POST와 upload가 허용되었는지 확인한다.
- [ ] upload store가 존재하고 directory인지 확인한다.
- [ ] raw body 업로드 정책을 구현한다.
- [ ] multipart boundary를 추출한다.
- [ ] multipart part의 header와 content를 분리한다.
- [ ] filename을 추출하고 검증한다.
- [ ] 파일 content를 binary-safe하게 저장한다.
- [ ] 신규 파일과 overwrite의 status를 구분한다.

filename 보안 검사:

```text
../
절대 경로
slash
backslash
제어 문자
빈 filename
```

실행 테스트:

```bash
curl -i -F file=@README.md http://127.0.0.1:8080/upload
cmp README.md www/uploads/README.md
```

완료 기준:

- [ ] text와 binary 파일이 원본과 동일하게 저장된다.
- [ ] upload off route는 업로드를 거절한다.
- [ ] 악성 filename으로 upload store 밖에 파일을 만들 수 없다.
- [ ] 신규 생성과 overwrite status가 정확하다.

---

### Step 14. DELETE 구현

목표: 허용된 route 내부의 파일만 안전하게 삭제한다.

먼저 만들 파일 예시:

```text
include/DeleteHandler.hpp
src/handlers/DeleteHandler.cpp
```

구현 순서:

- [ ] location에서 DELETE가 허용되었는지 확인한다.
- [ ] URI를 안전한 실제 파일 경로로 바꾼다.
- [ ] path traversal을 거절한다.
- [ ] target이 root 내부인지 확인한다.
- [ ] 파일 존재 여부와 종류를 확인한다.
- [ ] 일반 파일만 삭제한다.
- [ ] 성공 시 `204 No Content`를 반환한다.

실행 테스트:

```bash
curl -i -F file=@README.md http://127.0.0.1:8080/upload
curl -i -X DELETE http://127.0.0.1:8080/delete/README.md
curl -i -X DELETE http://127.0.0.1:8080/delete/README.md
```

완료 기준:

- [ ] 첫 삭제는 성공하고 두 번째 삭제는 `404`이다.
- [ ] directory와 root 자체는 삭제되지 않는다.
- [ ] traversal 요청으로 root 밖 파일을 삭제할 수 없다.

---

### Step 15. Chunked Request 구현

목표: `Transfer-Encoding: chunked` body를 해제하여 일반 body처럼 처리한다.

구현 순서:

- [ ] chunk size line을 16진수로 파싱한다.
- [ ] chunk data를 지정된 크기만큼 읽는다.
- [ ] chunk 뒤 CRLF를 검증한다.
- [ ] `0` chunk를 body 종료로 처리한다.
- [ ] framing을 제거한 decoded body를 만든다.
- [ ] decoded body 크기로 `client_max_body_size`를 검사한다.
- [ ] malformed chunk를 `400`으로 처리한다.
- [ ] CGI에는 decoded body를 전달한다.

반드시 테스트할 오류:

```text
잘못된 16진수 size
chunk data 길이 부족
CRLF 누락
Content-Length와 chunked 동시 사용
decoded body limit 초과
```

완료 기준:

- [ ] 여러 chunk를 올바른 body로 합친다.
- [ ] raw framing 크기가 아닌 decoded body 크기로 제한한다.
- [ ] malformed chunk 요청 후에도 서버가 살아 있다.

---

### Step 16. CGI 구현

목표: 최소 한 종류의 CGI를 non-blocking 방식으로 실행한다.

먼저 만들 파일 예시:

```text
include/CgiHandler.hpp
src/handlers/CgiHandler.cpp
www/cgi-bin/hello.py
```

구현 순서:

- [ ] location과 파일 확장자로 CGI 실행 여부를 판단한다.
- [ ] CGI stdin용 pipe와 stdout용 pipe를 만든다.
- [ ] pipe fd를 non-blocking으로 설정한다.
- [ ] `fork()`한다.
- [ ] child에서 stdin/stdout을 `dup2()`로 연결한다.
- [ ] child에서 불필요한 listen/client/pipe fd를 닫는다.
- [ ] 올바른 script directory로 `chdir()`한다.
- [ ] `execve()`로 interpreter와 script를 실행한다.
- [ ] parent의 CGI pipe를 기존 poll loop에 등록한다.
- [ ] request body를 CGI stdin에 partial write한다.
- [ ] body 전송 완료 후 stdin pipe를 닫아 EOF를 보낸다.
- [ ] CGI stdout을 partial read한다.
- [ ] CGI header와 body를 파싱한다.
- [ ] stdout EOF로 CGI output 완료를 판단한다.
- [ ] `waitpid(..., WNOHANG)`으로 child를 회수한다.
- [ ] CGI timeout과 실패 응답을 구현한다.

전달할 주요 환경변수:

```text
REQUEST_METHOD
SCRIPT_NAME
SCRIPT_FILENAME
QUERY_STRING
CONTENT_LENGTH
CONTENT_TYPE
SERVER_PROTOCOL
HTTP_*
```

실행 테스트:

```bash
curl -i 'http://127.0.0.1:8080/cgi-bin/hello.py?name=test'
curl -i -X POST -d 'name=test' http://127.0.0.1:8080/cgi-bin/hello.py
```

완료 기준:

- [ ] GET query와 POST body가 CGI에 전달된다.
- [ ] CGI 실행 중에도 다른 GET 요청이 처리된다.
- [ ] CGI가 Content-Length를 출력하지 않아도 stdout EOF로 완료된다.
- [ ] 무한 실행 CGI는 timeout 후 정리된다.
- [ ] CGI 실패 후에도 서버가 계속 동작한다.

---

### Step 17. Timeout과 자원 정리 강화

목표: 오류와 disconnect가 반복되어도 fd, 메모리, child가 누적되지 않게 한다.

구현 순서:

- [ ] client request/idle timeout을 구현한다.
- [ ] CGI timeout을 구현한다.
- [ ] client disconnect 시 관련 CGI를 종료하고 회수한다.
- [ ] 모든 close 경로에서 poll 목록과 map을 함께 정리한다.
- [ ] 모든 fork 성공/실패 경로의 pipe close를 확인한다.
- [ ] input, output, CGI output buffer 크기 제한을 정한다.
- [ ] 예외가 event loop 전체를 종료시키지 않게 한다.
- [ ] SIGPIPE로 서버가 종료되지 않게 처리한다.
- [ ] 종료 signal에서 fd와 child를 정리한다.

점검 방법:

```bash
valgrind --leak-check=full --show-leak-kinds=all --track-fds=yes \
  ./webserv config/default.conf
```

완료 기준:

- [ ] disconnect 반복 후 fd 수가 계속 증가하지 않는다.
- [ ] CGI timeout 반복 후 zombie process가 남지 않는다.
- [ ] malformed request 반복 후 서버가 정상 응답한다.
- [ ] 장시간 실행 후 메모리가 이유 없이 계속 증가하지 않는다.

---

### Step 18. 통합 테스트와 Stress Test 작성

목표: 한 명령으로 Mandatory 기능을 반복 검증한다.

먼저 만들 테스트 예시:

```text
tests/integration.sh
tests/malformed.py
tests/chunked.py
tests/slow_client.py
tests/cgi_nonblocking.py
tests/stress.py
```

통합 테스트 순서:

- [ ] 빌드와 불필요한 relink를 검사한다.
- [ ] 정상/비정상 config를 검사한다.
- [ ] 여러 포트의 서로 다른 콘텐츠를 검사한다.
- [ ] static GET과 binary 파일을 검사한다.
- [ ] 주요 status code를 검사한다.
- [ ] redirect, index, autoindex를 검사한다.
- [ ] body limit을 검사한다.
- [ ] upload 후 원본 byte와 비교한다.
- [ ] DELETE를 검사한다.
- [ ] chunked request를 검사한다.
- [ ] CGI GET/POST/timeout을 검사한다.
- [ ] slow client와 disconnect를 검사한다.
- [ ] 수백 개 동시 요청을 검사한다.

완료 기준:

- [ ] 자동 테스트를 여러 번 반복해도 결과가 같다.
- [ ] stress test 중에도 정상 요청이 성공한다.
- [ ] 테스트 후 서버가 계속 동작한다.
- [ ] Valgrind에서 memory/fd/child leak이 없다.

---

### Step 19. README와 평가 준비

목표: 실제 구현과 문서를 일치시키고 모든 핵심 코드를 설명할 수 있게 한다.

해야 할 일:

- [ ] 영어 `README.md` 첫 줄 형식을 확인한다.
- [ ] 실제 build/run 명령을 작성한다.
- [ ] 실제 config 문법을 설명한다.
- [ ] 실제 지원 기능만 적는다.
- [ ] 테스트 명령을 정리한다.
- [ ] 참고 자료와 AI 사용 범위를 적는다.
- [ ] 기존 `README.md`와 `TEST.md`의 미검증 설명을 실제 결과로 수정한다.
- [ ] 모든 팀원이 핵심 모듈을 직접 설명한다.

반드시 설명할 수 있어야 하는 내용:

```text
poll과 non-blocking 구조
partial recv/send
Client 상태 머신
Request Parser
Config와 longest-prefix location
URI와 실제 path 변환
upload와 multipart
DELETE 보안
chunked decoding
CGI pipe, EOF, timeout
disconnect와 fd 정리
status code 선택 기준
```

완료 기준:

- [ ] README 명령을 그대로 실행하면 프로젝트가 동작한다.
- [ ] 평가용 config 하나로 모든 Mandatory 기능을 시연할 수 있다.
- [ ] 예상 질문에 코드 위치를 가리키며 답할 수 있다.
- [ ] 짧은 기능 변경을 직접 구현할 수 있다.

---

### Step 20. Mandatory 완성 후 Bonus

Mandatory 테스트가 모두 통과한 경우에만 진행한다.

Bonus 후보:

- [ ] Cookie parsing과 `Set-Cookie`.
- [ ] Session id와 session data.
- [ ] Session TTL과 최대 개수 제한.
- [ ] 여러 CGI 확장자와 interpreter.

Bonus 완료 기준:

- [ ] Bonus 추가 후에도 모든 Mandatory 테스트가 그대로 통과한다.
- [ ] session 또는 CGI 자원이 무한히 증가하지 않는다.

---

### 지금 당장 시작할 첫 작업

현재 상태에서 가장 먼저 실행할 체크리스트:

- [ ] [STEP00.md](STEP00.md)를 열고 config 문법과 root 매핑 규칙을 결정한다.
- [ ] `include`, `src`, `config`, `www`, `tests` 디렉토리를 만든다.
- [ ] [STEP01.md](STEP01.md)를 따라 `src/main.cpp`와 `Makefile`을 만든다.
- [ ] `make`, 두 번째 `make`, `make re`를 검증한다.
- [ ] [STEP02.md](STEP02.md)를 따라 `127.0.0.1:8080`에서 고정 응답을 보낸다.

Step 01이 완료되기 전에는 HTTP parser, config parser, CGI 구현을 동시에 시작하지 않는다.

---

## 1. 현재 디렉토리 상태

### 1.1 실제로 존재하는 것

현재 디렉토리에는 과제 PDF와 설명용 Markdown 문서만 존재한다.

```text
en.subject.pdf
GUIDE.md
SETTING.md
STRUCTURE.md
README.md
README_KR.md
TEST.md
NGINX.md
html.md
STEP.md
STEP00.md ~ STEP20.md
```

`STEP00.md`부터 `STEP20.md`는 이 문서의 실행 계획을 단계별 독립 문서로 분리한 상세 체크리스트이다.

### 1.2 아직 존재하지 않는 것

현재 파일 목록 기준으로 다음 구현 산출물은 없다.

```text
Makefile
include/
src/
config/
www/
tests/
webserv 실행 파일
```

따라서 `TEST.md`와 `README.md`에 적힌 기존 구현, 자동 테스트 통과, `src/Server.cpp`, `config/default.conf` 등의 설명은 **현재 디렉토리에서 검증된 사실이 아니다**. 구현을 완료한 뒤 실제 결과에 맞게 다시 검증하고 문서를 수정해야 한다.

### 1.3 지금부터 만들어야 할 최종 결과

최소 제출 결과는 다음 형태가 되어야 한다.

```text
.
├── Makefile
├── README.md
├── include/
├── src/
├── config/
├── www/
├── tests/
└── webserv                 # make 후 생성
```

---

## 2. 과제의 최종 목표

사용자가 구현할 프로그램은 브라우저 및 HTTP 클라이언트와 통신할 수 있는 **C++98 기반 non-blocking HTTP 서버**이다.

전체 처리 흐름:

```text
설정 파일 파싱
→ 여러 listen socket 생성
→ 하나의 poll 기반 이벤트 루프 시작
→ 클라이언트 연결 accept
→ HTTP 요청을 조각 단위로 수신
→ 요청 완성 여부 판단 및 파싱
→ server/location 설정 선택
→ GET / POST / DELETE / upload / CGI 처리
→ HTTP 응답 생성
→ 응답을 조각 단위로 전송
→ 연결 종료 또는 다음 요청 처리
```

서버는 정상 요청만 처리하는 프로그램이 아니다. 느린 클라이언트, 잘못된 요청, 중간 연결 종료, 큰 body, CGI 실패가 발생해도 전체 서버는 계속 동작해야 한다.

---

## 3. Mandatory 요구사항 요약

다음 항목은 최종적으로 반드시 만족해야 한다.

### 3.1 빌드와 실행

- [ ] 실행 파일 이름은 `webserv`이다.
- [ ] 실행 형식은 `./webserv [configuration file]`이다.
- [ ] 설정 파일을 인자로 받거나, 인자가 없을 때 기본 설정 파일을 사용한다.
- [ ] `Makefile`에 `$(NAME)`, `all`, `clean`, `fclean`, `re` 규칙이 있다.
- [ ] 불필요한 relink가 발생하지 않는다.
- [ ] `c++ -Wall -Wextra -Werror -std=c++98`로 컴파일된다.
- [ ] 외부 라이브러리와 Boost를 사용하지 않는다.
- [ ] 다른 웹 서버를 `execve()`하지 않는다.
- [ ] `fork()`는 CGI 실행에만 사용한다.

### 3.2 서버와 I/O

- [ ] 서버는 항상 non-blocking으로 동작한다.
- [ ] listen socket을 포함한 socket/pipe I/O를 하나의 `poll()` 또는 equivalent 이벤트 루프로 관리한다.
- [ ] 이벤트 감시 함수는 읽기와 쓰기를 동시에 감시한다.
- [ ] socket, pipe 등 대기 가능한 fd는 readiness 이벤트 없이 읽거나 쓰지 않는다.
- [ ] read/write 후 `errno` 값을 보고 서버 동작을 조정하지 않는다.
- [ ] 일반 디스크 파일은 poll 대상에서 제외할 수 있다.
- [ ] client disconnect를 안전하게 처리한다.
- [ ] 요청이 무한히 대기하지 않도록 timeout을 둔다.
- [ ] stress 상황에서도 서버가 계속 동작한다.

### 3.3 HTTP 기능

- [ ] 표준 브라우저에서 사용할 수 있다.
- [ ] HTTP status code를 상황에 맞게 정확히 반환한다.
- [ ] 설정된 에러 페이지가 없을 때 기본 에러 페이지를 제공한다.
- [ ] 완전한 정적 웹사이트를 제공한다.
- [ ] 최소 `GET`, `POST`, `DELETE`를 지원한다.
- [ ] 파일 업로드를 지원한다.
- [ ] 여러 포트에서 서로 다른 콘텐츠를 제공한다.

### 3.4 설정 파일 기능

- [ ] 모든 listen `interface:port` 쌍을 정의할 수 있다.
- [ ] 기본/custom error page를 설정할 수 있다.
- [ ] request body 최대 크기를 설정할 수 있다.
- [ ] route별 허용 method를 설정할 수 있다.
- [ ] route별 redirect를 설정할 수 있다.
- [ ] route별 root directory를 설정할 수 있다.
- [ ] directory listing을 켜거나 끌 수 있다.
- [ ] directory 요청의 기본 index 파일을 설정할 수 있다.
- [ ] route별 upload 허용 여부와 저장 위치를 설정할 수 있다.
- [ ] 파일 확장자 기반 CGI 실행을 설정할 수 있다.
- [ ] 최소 한 종류의 CGI를 지원한다.
- [ ] 모든 기능을 평가에서 시연할 config와 기본 파일을 제공한다.

### 3.5 README와 제출

- [ ] Git repository 루트에 영어 `README.md`가 있다.
- [ ] 첫 줄은 지정된 italic 문장 형식이다.
- [ ] `Description`, `Instructions`, `Resources` 섹션이 있다.
- [ ] AI를 어떤 작업에 사용했는지 기록한다.
- [ ] README에 적힌 기능과 경로가 실제 구현과 일치한다.
- [ ] 제출 repository에 필요한 파일이 모두 포함되어 있다.

### 3.6 공식 명세에 기재된 external functions

공식 PDF에 기재된 함수 목록은 다음과 같다. 시스템 호출이나 C 함수가 추가로 필요해 보일 때는 이 목록과 실제 평가표를 먼저 확인한다.

```text
execve, pipe, strerror, gai_strerror, errno, dup, dup2, fork, socketpair,
htons, htonl, ntohs, ntohl,
select, poll, epoll_create, epoll_ctl, epoll_wait, kqueue, kevent,
socket, accept, listen, send, recv, chdir, bind, connect,
getaddrinfo, freeaddrinfo, setsockopt, getsockname, getprotobyname,
fcntl, close, read, write, waitpid, kill, signal, access, stat, open,
opendir, readdir, closedir
```

대표 용도:

| 영역 | 주요 함수 |
|---|---|
| 주소와 listen socket | `getaddrinfo`, `socket`, `setsockopt`, `bind`, `listen` |
| client 통신 | `accept`, `recv`, `send`, `close` |
| 이벤트 루프 | `poll` 또는 equivalent |
| non-blocking 설정 | `fcntl` |
| 일반 파일과 directory | `open`, `read`, `write`, `stat`, `access`, `opendir`, `readdir`, `closedir` |
| CGI | `pipe`, `fork`, `dup2`, `chdir`, `execve`, `waitpid`, `kill` |

macOS에서는 `fcntl()`을 `F_SETFL`, `O_NONBLOCK`, `FD_CLOEXEC` 범위로만 사용하라는 추가 제한이 있다. 다른 환경에서도 평가 호환성을 위해 불필요한 flag 사용을 피한다.

---

## 4. 절대 어기면 안 되는 핵심 규칙

### 4.1 하나의 이벤트 루프

가장 중요한 설계 원칙:

```text
listen socket, client socket, CGI stdin/stdout pipe 등
대기할 수 있는 모든 fd의 I/O는
하나의 poll() 또는 equivalent 호출 흐름에서 관리한다.
```

금지되는 구조:

```text
client 하나를 처리하는 동안 blocking read
응답 전체가 전송될 때까지 blocking write
CGI 출력이 끝날 때까지 blocking read
CGI 종료까지 blocking waitpid
client별 thread 또는 CGI 외 목적의 fork
```

### 4.2 readiness 없이 I/O 금지

다음 원칙을 지킨다.

```text
POLLIN  확인 후 accept/recv/read
POLLOUT 확인 후 send/write
CGI 종료 확인은 waitpid(..., WNOHANG)
```

regular disk file은 readiness 감시 예외이므로 `open/read/write/stat/opendir/readdir` 등을 직접 사용할 수 있다.

### 4.3 read와 write는 한 번에 끝나지 않는다

TCP는 요청 경계를 보장하지 않는다.

```text
요청 하나가 여러 recv에 나뉠 수 있다.
요청 여러 개가 recv 한 번에 들어올 수 있다.
응답 전체를 send 한 번에 보내지 못할 수 있다.
```

따라서 client마다 다음을 반드시 저장한다.

```text
input buffer
현재 parser 상태
완성된 request 또는 남은 입력
output buffer
현재까지 전송한 offset
connection 상태
마지막 활동 시간
```

### 4.4 서버는 에러가 나도 살아 있어야 한다

클라이언트 하나의 오류는 해당 연결의 에러 응답 또는 정리로 끝나야 한다.

```text
잘못된 요청 → 해당 client에 4xx 응답
파일 접근 실패 → 해당 client에 4xx/5xx 응답
CGI 실패 → 해당 client에 5xx 응답
client disconnect → 관련 fd와 CGI 정리
다른 client → 계속 정상 처리
```

공식 일반 규칙은 메모리 부족을 포함해 어떤 상황에서도 프로그램이 crash하거나 예기치 않게 종료되면 안 된다고 명시한다. 현실적으로 할 수 있는 범위에서 다음을 적용한다.

- allocation과 container 확장 실패가 event loop 전체를 비정상 종료시키지 않게 한다.
- 처리할 수 없는 자원 부족은 새 연결 또는 해당 요청을 정리하는 방식으로 격리한다.
- top-level과 request 처리 경계에서 예외 정책을 명확히 한다.
- cleanup 함수 자체가 예외를 던지지 않게 설계한다.

---

## 5. 먼저 결정할 설계

코드를 쓰기 전에 팀원이 합의하고 문서화해야 할 항목이다.

### 5.1 이벤트 감시 방식

권장 선택:

```text
poll()
```

이유:

- subject와 평가 설명에 직접 등장한다.
- listen/client/CGI pipe를 같은 배열에서 관리하기 쉽다.
- `select()`의 fd 제한 및 fd set 재구성보다 구조를 설명하기 쉽다.

### 5.2 연결 정책

초기 구현은 응답마다 `Connection: close`를 사용하면 단순하다. 이후 구조가 안정되면 keep-alive를 추가할 수 있다.

keep-alive를 구현한다면:

- 한 응답 전송 후 input buffer의 남은 데이터를 보존한다.
- parser/request/response 상태만 초기화한다.
- HTTP/1.0과 HTTP/1.1의 기본 연결 정책을 구분한다.
- idle timeout을 둔다.

### 5.3 설정 문법

NGINX의 `server` 블록을 참고하되, 직접 파싱 가능한 단순 문법으로 고정한다.

권장 예시:

```conf
server {
    listen 127.0.0.1:8080;
    root ./www;
    index index.html;
    client_max_body_size 10M;

    error_page 404 /errors/404.html;
    error_page 500 /errors/500.html;

    location / {
        methods GET;
        autoindex off;
    }

    location /upload {
        methods GET POST DELETE;
        upload on;
        upload_store ./www/uploads;
    }

    location /old {
        return 301 /;
    }

    location /cgi-bin {
        methods GET POST;
        root ./www/cgi-bin;
        cgi .py /usr/bin/python3;
    }
}

server {
    listen 127.0.0.1:8081;
    root ./www/site_b;
    index index.html;
}
```

### 5.4 path 매핑 규칙

location root의 의미를 한 가지로 정하고 모든 handler가 같은 함수를 사용해야 한다.

이 문서에서 권장하는 규칙:

```text
요청 URI: /kapouet/pouic/toto
location: /kapouet
location root: /tmp/www
실제 파일: /tmp/www/pouic/toto
```

즉, location prefix를 제거한 나머지 URI를 location root 뒤에 붙인다. server root를 상속할 때의 규칙도 테스트로 고정한다.

### 5.5 status code 정책

대표적인 기준을 미리 정한다.

| 상황 | 권장 status |
|---|---:|
| 정상 GET | `200 OK` |
| 새 업로드 생성 | `201 Created` |
| body 없는 삭제 성공 | `204 No Content` |
| redirect | `301`, `302`, `307`, `308` |
| 잘못된 request 문법 | `400 Bad Request` |
| 접근 금지, autoindex off, upload off | `403 Forbidden` |
| 파일/route 없음 | `404 Not Found` |
| 알려진 method지만 route에서 금지 | `405 Method Not Allowed` + `Allow` |
| body 제한 초과 | `413 Payload Too Large` |
| URI가 지나치게 김 | `414 URI Too Long` |
| 서버가 구현하지 않은 method | `501 Not Implemented` |
| CGI/interpreter 실패 또는 잘못된 CGI 출력 | `502 Bad Gateway` |
| CGI timeout | `504 Gateway Timeout` |
| 지원하지 않는 HTTP version | `505 HTTP Version Not Supported` |

`405`와 `501`은 구분한다. `GET/POST/DELETE`처럼 서버가 아는 method가 특정 route에서 금지되면 `405`, 서버가 구현하지 않은 method라면 `501`이 자연스럽다.

---

## 6. 권장 디렉토리 구조

처음에 다음 구조를 만들고 역할을 분리한다.

```text
.
├── Makefile
├── README.md
├── include
│   ├── ConfigParser.hpp
│   ├── ConfigTypes.hpp
│   ├── Server.hpp
│   ├── Client.hpp
│   ├── HttpRequest.hpp
│   ├── RequestParser.hpp
│   ├── HttpResponse.hpp
│   ├── ResponseBuilder.hpp
│   ├── Router.hpp
│   ├── StaticFileHandler.hpp
│   ├── UploadHandler.hpp
│   ├── DeleteHandler.hpp
│   ├── CgiHandler.hpp
│   ├── ErrorPageHandler.hpp
│   └── Utils.hpp
├── src
│   ├── main.cpp
│   ├── config
│   ├── core
│   ├── http
│   ├── handlers
│   └── utils
├── config
│   ├── default.conf
│   └── invalid/
├── www
│   ├── index.html
│   ├── style.css
│   ├── upload.html
│   ├── errors
│   ├── uploads
│   ├── cgi-bin
│   └── site_b
└── tests
    ├── integration.sh
    ├── malformed.py
    ├── chunked.py
    ├── slow_client.py
    ├── cgi_nonblocking.py
    └── stress.py
```

파일을 반드시 이 이름으로 만들 필요는 없지만, 아래 책임은 명확히 분리해야 한다.

| 모듈 | 책임 |
|---|---|
| `ConfigParser` | config tokenize, parse, 문법/값 검증 |
| `ConfigTypes` | server/location/listen/CGI 설정 저장 |
| `Server` 또는 `EventLoop` | listen/client/CGI fd 등록과 poll loop |
| `Client` | 연결별 buffer, offset, parser, timeout, 상태 |
| `RequestParser` | request line, header, Content-Length/chunked body 파싱 |
| `Router` | listen socket의 server 선택, longest-prefix location 선택, path 계산 |
| `ResponseBuilder` | status line, headers, body, error response 생성 |
| handlers | static, upload, DELETE, CGI 등 실제 기능 |
| utils | 문자열, MIME, 파일 상태, URL/path 정규화 |

---

## 7. 권장 핵심 자료구조

구체적인 이름은 바꿔도 되지만 아래 정보는 필요하다.

### 7.1 설정 구조

```cpp
struct ListenConfig {
    std::string host;
    int port;
};

struct RedirectConfig {
    bool enabled;
    int statusCode;
    std::string target;
};

struct CgiConfig {
    std::map<std::string, std::string> interpreters;
};

struct LocationConfig {
    std::string path;
    std::set<std::string> methods;
    bool hasRoot;
    std::string root;
    bool hasIndex;
    std::vector<std::string> indexFiles;
    bool autoindex;
    bool uploadEnabled;
    std::string uploadStore;
    RedirectConfig redirect;
    CgiConfig cgi;
};

struct ServerConfig {
    std::vector<ListenConfig> listens;
    std::string root;
    std::vector<std::string> indexFiles;
    std::map<int, std::string> errorPages;
    size_t clientMaxBodySize;
    std::vector<LocationConfig> locations;
};
```

설정이 명시되지 않은 상태와 `off`, 빈 문자열을 구분해야 한다. 그래야 server 값 상속과 location override가 정확해진다.

### 7.2 HTTP request

```cpp
struct HttpRequest {
    std::string method;
    std::string target;
    std::string path;
    std::string query;
    std::string version;
    std::map<std::string, std::string> headers;
    std::string body;
};
```

추가로 다음 정보가 유용하다.

```text
body framing 방식: 없음 / Content-Length / chunked
expected body size
decoded body size
keep-alive 여부
파싱 에러 status
```

### 7.3 client 연결 상태

권장 상태 예시:

```cpp
enum ClientState {
    READING_HEADERS,
    READING_BODY,
    PROCESSING_REQUEST,
    RUNNING_CGI,
    WRITING_RESPONSE,
    CLOSING
};
```

Client가 저장할 값:

```text
client fd
어떤 listen fd로 들어왔는지
선택된 server config
input buffer
request parser와 request
response buffer
send offset
state
keep-alive 여부
마지막 활동 시간
연결된 CGI 작업 식별자
```

### 7.4 CGI 작업 상태

```text
child pid
stdin pipe write fd
stdout pipe read fd
request body와 write offset
CGI output buffer
시작 시간
stdin 종료 여부
stdout EOF 여부
child 회수 여부
연결된 client fd
```

---

## 8. 구현 단계별 상세 참고

이 절부터는 앞의 **Step 00~20 실행 계획을 진행할 때 참고하는 상세 설명**이다. 실제 작업 순서와 진행 체크는 문서 앞부분의 Step-by-Step 실행 계획을 기준으로 한다.

각 상세 단계는 **구현 작업 → 주의사항 → 완료 조건 → 최소 테스트** 순서로 정리되어 있다.

---

## STEP 0. 저장소 골격과 빌드 시스템 만들기

### 구현 작업

- [ ] 권장 디렉토리 구조를 만든다.
- [ ] `Makefile`을 작성한다.
- [ ] `NAME := webserv`로 설정한다.
- [ ] 의존성 파일 또는 올바른 object 규칙을 사용해 불필요한 relink를 막는다.
- [ ] `main.cpp`에서 인자 개수를 검증한다.
- [ ] config 인자가 없을 때의 정책을 정한다.
- [ ] 치명적 초기화 오류는 명확한 메시지와 non-zero status로 종료한다.
- [ ] SIGPIPE로 서버 전체가 종료되지 않도록 정책을 세운다.

### 주의사항

- C++11 기능을 사용하지 않는다.
- 외부 parser/network 라이브러리를 추가하지 않는다.
- 허용 함수 목록에 없는 시스템 호출을 무심코 사용하지 않는다.
- `fork()`는 나중의 CGI 외에는 사용하지 않는다.

### 완료 조건

```bash
make
make
make clean
make re
```

- 첫 `make`가 성공한다.
- 두 번째 `make`가 relink하지 않는다.
- `make re` 후 `webserv`가 생성된다.
- `-Wall -Wextra -Werror -std=c++98`에서 경고가 없다.

---

## STEP 1. 최소 TCP 서버 만들기

### 구현 작업

- [ ] `socket()`으로 listen socket을 만든다.
- [ ] `setsockopt()`로 필요한 socket option을 설정한다.
- [ ] `bind()`로 host/port를 연결한다.
- [ ] `listen()`으로 연결 대기 상태로 만든다.
- [ ] fd를 `O_NONBLOCK`으로 설정한다.
- [ ] 새 연결을 `accept()`한다.
- [ ] 고정된 올바른 HTTP 응답을 보낸다.

최소 응답 예:

```http
HTTP/1.1 200 OK\r
Content-Type: text/plain\r
Content-Length: 12\r
Connection: close\r
\r
Hello World!
```

### 주의사항

이 단계의 목적은 socket 흐름 확인이지만, 최종 구조에서는 accept/read/write가 모두 poll readiness 뒤에서 일어나야 한다. 임시 blocking 코드를 오래 유지하지 않는다.

### 완료 조건

- 브라우저에서 페이지가 열린다.
- `curl -i`로 status line, header, body가 보인다.
- 응답의 `Content-Length`가 실제 body byte 수와 일치한다.

### 최소 테스트

```bash
curl -i http://127.0.0.1:8080/
printf 'GET / HTTP/1.1\r\nHost: localhost\r\n\r\n' | nc 127.0.0.1 8080
```

---

## STEP 2. 단일 poll 이벤트 루프 구현

### 구현 작업

- [ ] 모든 listen fd를 poll 목록에 등록한다.
- [ ] listen fd의 `POLLIN`에서만 `accept()`한다.
- [ ] accept한 client fd를 non-blocking으로 설정한다.
- [ ] client fd를 poll 목록에 등록한다.
- [ ] client의 `POLLIN`에서만 `recv()`한다.
- [ ] 응답이 준비된 client만 `POLLOUT`을 감시한다.
- [ ] client의 `POLLOUT`에서만 `send()`한다.
- [ ] `POLLERR`, `POLLHUP`, `POLLNVAL` 및 disconnect를 처리한다.
- [ ] fd를 닫을 때 poll 목록과 모든 관련 map에서 제거한다.
- [ ] close를 한 곳의 정리 함수로 통합한다.

### 주의사항

- 매 loop에서 모든 client에게 무조건 send하지 않는다.
- response가 없는 client에 계속 `POLLOUT`을 등록하면 busy loop가 발생할 수 있다.
- vector에서 fd를 제거할 때 index 무효화에 주의한다.
- read/write 뒤 `errno`를 근거로 재시도 흐름을 만들지 않는다.

### 완료 조건

- 여러 클라이언트가 동시에 접속해도 각각 응답한다.
- 연결 직후 아무것도 보내지 않는 client가 있어도 다른 요청이 처리된다.
- client가 중간에 끊겨도 서버가 종료되지 않는다.
- CPU를 불필요하게 100% 사용하지 않는다.

### 최소 테스트

```bash
curl http://127.0.0.1:8080/ &
curl http://127.0.0.1:8080/ &
curl http://127.0.0.1:8080/ &
```

추가로 `nc 127.0.0.1 8080` 연결을 열어둔 상태에서 다른 curl 요청을 보낸다.

---

## STEP 3. Client buffer와 상태 머신 구현

### 구현 작업

- [ ] client fd별 `Client` 객체를 만든다.
- [ ] recv 결과를 input buffer에 byte 수 기준으로 append한다.
- [ ] send 결과만큼 send offset을 증가시킨다.
- [ ] 응답이 남아 있으면 다음 `POLLOUT`을 기다린다.
- [ ] 상태 전환을 명확히 구현한다.
- [ ] 마지막 활동 시간을 갱신한다.
- [ ] client timeout 시 관련 자원을 정리한다.

권장 흐름:

```text
READING_HEADERS
→ READING_BODY
→ PROCESSING_REQUEST
→ RUNNING_CGI 또는 WRITING_RESPONSE
→ CLOSING 또는 READING_HEADERS
```

### 완료 조건

- 요청을 한 글자씩 나눠 보내도 완성된 뒤 응답한다.
- 큰 응답을 partial send로 끝까지 전송한다.
- 느린 client가 다른 client를 막지 않는다.
- timeout이 발생해도 서버가 계속 동작한다.

---

## STEP 4. HTTP request parser 구현

### 구현 작업

- [ ] raw 입력에서 `\r\n\r\n`을 찾아 header 끝을 판단한다.
- [ ] request line을 `method target version`으로 파싱한다.
- [ ] target을 path와 query string으로 분리한다.
- [ ] header를 case-insensitive key 기준으로 저장하거나 조회한다.
- [ ] header name/value 문법을 검증한다.
- [ ] `Content-Length`를 안전하게 숫자로 변환한다.
- [ ] HTTP/1.1에서 `Host` 누락을 거절한다.
- [ ] 지원 HTTP version을 결정하고 그 외는 거절한다.
- [ ] header와 request line 최대 크기를 둔다.
- [ ] body가 완성될 때까지 처리하지 않는다.
- [ ] 첫 request 뒤에 남은 bytes를 다음 request용으로 보존한다.

### 파서 상태

```text
REQUEST_LINE
HEADERS
BODY_BY_LENGTH
BODY_CHUNK_SIZE
BODY_CHUNK_DATA
BODY_CHUNK_CRLF
BODY_CHUNK_TRAILERS
COMPLETE
ERROR
```

### 반드시 검사할 잘못된 요청

- [ ] request line token 개수가 잘못됨.
- [ ] method 또는 version이 비어 있음.
- [ ] header에 colon이 없음.
- [ ] 중복되거나 충돌하는 body framing header.
- [ ] 잘못된 `Content-Length`.
- [ ] body가 선언된 길이와 일치하지 않음.
- [ ] 지나치게 큰 header/URI/body.
- [ ] HTTP/1.1인데 `Host`가 없음.

### 완료 조건

- 요청이 여러 recv로 나뉘어도 파싱한다.
- body가 덜 왔을 때 기다린다.
- malformed request에 적절한 4xx 응답을 만들 수 있다.
- binary body 안의 `\0`도 `std::string::size()` 기준으로 보존한다.

### 최소 테스트

```bash
printf 'GET /hello?name=kyu HTTP/1.1\r\nHost: localhost\r\nX-Test: yes\r\n\r\n' \
  | nc 127.0.0.1 8080
```

Python socket으로 request를 여러 조각에 나눠 보내는 테스트도 작성한다.

---

## STEP 5. HTTP response 모델과 공통 에러 처리 구현

### 구현 작업

- [ ] status code와 reason phrase 표를 만든다.
- [ ] `HttpResponse`에 status, headers, body를 저장한다.
- [ ] raw HTTP response 직렬화 함수를 만든다.
- [ ] body가 있는 응답의 `Content-Length`를 정확히 계산한다.
- [ ] `Content-Type`, `Connection` 등 공통 header를 설정한다.
- [ ] body가 없어야 하는 응답을 구분한다.
- [ ] 기본 HTML error page 생성기를 만든다.
- [ ] custom error page가 있으면 읽고, 실패하면 기본 페이지로 fallback한다.

### 주의사항

- 응답 header 끝은 반드시 `\r\n\r\n`이다.
- `Content-Length`는 문자 개수가 아니라 실제 body byte 수이다.
- `204 No Content` 등에 body를 넣지 않는다.
- 에러 페이지를 읽다가 다시 에러가 나도 재귀적으로 무한 호출하지 않는다.

### 완료 조건

- 모든 에러 경로가 공통 response builder를 사용한다.
- browser가 응답 후 계속 로딩하지 않는다.
- custom error file이 없어도 기본 에러 응답을 보낸다.

---

## STEP 6. Config tokenizer와 parser 구현

### 구현 작업

- [ ] `{`, `}`, `;`, 일반 token을 구분하는 tokenizer를 만든다.
- [ ] 하나 이상의 `server` block을 파싱한다.
- [ ] server-level directive를 파싱한다.
- [ ] 중첩된 `location` block을 파싱한다.
- [ ] 알 수 없는 directive를 오류 처리한다.
- [ ] 잘못된 중괄호/semicolon을 오류 처리한다.
- [ ] 중복 directive 정책을 정한다.
- [ ] 필수값과 기본값을 정한다.
- [ ] 설정 전체를 먼저 검증한 뒤 listen socket을 연다.

### 최소 지원 directive

```text
server
listen
root
index
client_max_body_size
error_page
location
methods
return
autoindex
upload
upload_store
cgi
```

### 값 검증

- [ ] port는 숫자이며 `1..65535` 범위이다.
- [ ] body size는 음수가 아니며 overflow하지 않는다.
- [ ] body size 단위 정책이 명확하다.
- [ ] error status가 유효하고 path가 존재 가능한 형태이다.
- [ ] methods는 허용된 token만 포함한다.
- [ ] `upload on`이면 `upload_store`가 설정되어 있다.
- [ ] CGI extension과 interpreter 경로가 유효한 형태이다.
- [ ] 동일 interface:port 중복 binding 정책이 명확하다.

### 완료 조건

- 정상 config는 내부 객체로 변환된다.
- 잘못된 config는 서버를 시작하지 않고 명확히 실패한다.
- parser가 공백과 줄바꿈 차이에 흔들리지 않는다.

### 테스트 config

정상 config뿐 아니라 다음 invalid config 파일을 만든다.

```text
닫히지 않은 server block
semicolon 누락
숫자가 아닌 port
범위 밖 port
음수 body size
알 수 없는 directive
지원하지 않는 method
upload_store 없는 upload on
인자가 부족하거나 너무 많은 directive
```

---

## STEP 7. 여러 listen socket과 server 선택 구현

### 구현 작업

- [ ] config의 모든 interface:port에 listen socket을 생성한다.
- [ ] listen fd와 해당 server config를 연결한다.
- [ ] 모든 listen fd를 같은 poll loop에 등록한다.
- [ ] accept한 client에 어느 listen fd로 들어왔는지 기록한다.
- [ ] 포트별로 다른 root/content를 제공한다.

### 범위 주의

Virtual host는 subject에서 scope 밖이다. 같은 interface:port의 여러 `server_name` 선택은 mandatory가 아니므로, mandatory 완성 전에는 구현하지 않아도 된다.

### 완료 조건

```text
127.0.0.1:8080 → site A
127.0.0.1:8081 → site B
```

두 사이트가 하나의 `webserv` 프로세스에서 동시에 동작한다.

---

## STEP 8. Router와 location 매칭 구현

### 구현 작업

- [ ] request가 들어온 listen fd를 기준으로 server config를 선택한다.
- [ ] request path와 일치하는 location을 찾는다.
- [ ] regex 없이 longest-prefix match를 구현한다.
- [ ] server 기본값과 location override를 합쳐 effective config를 만든다.
- [ ] redirect를 가장 먼저 확인한다.
- [ ] method 허용 여부를 확인한다.
- [ ] body size 제한을 적용한다.
- [ ] CGI/upload/static handler 중 처리 대상을 결정한다.
- [ ] URI를 실제 파일 경로로 안전하게 변환한다.

### 권장 처리 우선순위

```text
1. listen fd 기준 server 선택
2. longest-prefix location 선택
3. effective config 계산
4. redirect 확인
5. method 검사
6. body size 검사
7. CGI 또는 upload 여부 결정
8. 실제 파일 경로 계산
9. 파일/디렉토리 상태 확인
10. directory면 index 확인
11. index가 없으면 autoindex 확인
12. 에러 페이지 또는 정상 response 생성
```

### 완료 조건

- `/`, `/images`, `/images/icons`가 있을 때 가장 긴 일치 location을 선택한다.
- location 값이 없을 때 server 값을 상속한다.
- redirect route에서는 파일/CGI 처리를 하지 않고 즉시 응답한다.
- 금지된 method에 `405`와 `Allow` header를 반환한다.

---

## STEP 9. 정적 파일 GET 구현

### 구현 작업

- [ ] URI를 안전한 실제 파일 경로로 변환한다.
- [ ] query string은 파일 경로에서 제외한다.
- [ ] `stat()`으로 존재 여부와 파일 종류를 확인한다.
- [ ] regular file을 binary-safe하게 읽는다.
- [ ] 확장자별 MIME type을 설정한다.
- [ ] 파일 없음, 권한 없음, 디렉토리를 구분한다.
- [ ] 큰 파일 응답도 partial send로 전송한다.

### 최소 MIME type

```text
.html → text/html
.css  → text/css
.js   → application/javascript
.txt  → text/plain
.jpg/.jpeg → image/jpeg
.png  → image/png
.gif  → image/gif
.ico  → image/x-icon
그 외 → application/octet-stream
```

### 완료 조건

- HTML, CSS, JS, 이미지로 구성된 정적 사이트가 브라우저에서 정상 표시된다.
- binary 파일이 손상되지 않는다.
- 없는 파일은 `404`, 접근 불가는 `403`으로 처리한다.

---

## STEP 10. 디렉토리 index와 autoindex 구현

### 구현 작업

- [ ] 실제 경로가 directory인지 확인한다.
- [ ] 설정된 index 파일을 순서대로 찾는다.
- [ ] index가 있으면 해당 파일을 제공한다.
- [ ] index가 없고 autoindex가 켜져 있으면 목록 HTML을 생성한다.
- [ ] index가 없고 autoindex가 꺼져 있으면 `403`을 반환한다.
- [ ] autoindex link가 현재 URI 기준으로 올바르게 동작하게 만든다.
- [ ] HTML 특수 문자를 escape한다.

### 처리 우선순위

```text
index file
→ autoindex on이면 listing
→ autoindex off이면 403
```

### 완료 조건

- directory 내 index가 있으면 listing보다 index가 우선한다.
- autoindex page의 파일/폴더 link로 이동할 수 있다.
- 숨겨야 할 경로나 root 밖 경로를 노출하지 않는다.

---

## STEP 11. Content-Length body와 크기 제한 구현

### 구현 작업

- [ ] `Content-Length`를 header 단계에서 검사한다.
- [ ] 제한보다 큰 요청은 body 전체를 메모리에 쌓기 전에 `413`으로 처리한다.
- [ ] body를 받는 중에도 decoded body 누적 크기를 검사한다.
- [ ] body가 정확히 완성된 뒤 handler를 호출한다.
- [ ] body 최대 크기와 header 최대 크기를 별도 제한으로 관리한다.

### 주의사항

`client_max_body_size`는 request 전체 raw buffer 크기가 아니라 **실제 decoded body 크기**에 적용해야 한다. 특히 chunked framing bytes 때문에 정상 body가 잘못 거절되지 않게 한다.

### 완료 조건

- 제한 이하 body는 정상 처리한다.
- 제한 초과 body는 `413`을 반환한다.
- 큰 body를 천천히 보내도 서버가 다른 요청을 처리한다.

---

## STEP 12. POST와 파일 업로드 구현

### 구현 작업

- [ ] 일반 raw body POST의 동작을 정의한다.
- [ ] `multipart/form-data`의 boundary를 파싱한다.
- [ ] 각 part의 header와 content를 binary-safe하게 분리한다.
- [ ] 파일 part의 filename을 추출한다.
- [ ] 업로드가 허용된 route인지 확인한다.
- [ ] upload store가 존재하고 directory이며 쓰기 가능한지 확인한다.
- [ ] 파일을 저장한다.
- [ ] 새 파일 생성과 기존 파일 overwrite의 status 정책을 구분한다.
- [ ] 업로드 실패 시 적절한 에러를 반환한다.

### 보안 검사

- [ ] filename의 `/`, `\`, `..`, 제어 문자, 절대 경로를 거절하거나 안전하게 정제한다.
- [ ] URL decode 이후에도 traversal을 다시 검사한다.
- [ ] upload store 밖으로 나가지 못하게 한다.
- [ ] 빈 filename과 중복 filename 정책을 정한다.

### status 정책 예시

```text
새 파일 생성 → 201 Created
기존 파일 교체 → 200 OK 또는 204 No Content
upload off → 403 Forbidden
body 제한 초과 → 413 Payload Too Large
저장 경로 접근 불가 → 403 또는 500
```

### 완료 조건

- text와 binary 파일이 원본과 같은 byte로 저장된다.
- multipart body가 recv 여러 번으로 나뉘어도 정상 저장된다.
- 악성 filename으로 root 밖 파일을 만들 수 없다.

---

## STEP 13. DELETE 구현

### 구현 작업

- [ ] route에서 DELETE가 허용되었는지 확인한다.
- [ ] URI를 안전한 실제 파일 경로로 변환한다.
- [ ] target이 root 안에 있는지 검증한다.
- [ ] 파일 존재 여부와 종류를 확인한다.
- [ ] 일반 파일만 삭제할지 정책을 정한다.
- [ ] 성공/실패 status를 정확히 반환한다.

### 주의사항

- `/`, upload root 자체, directory를 실수로 삭제하지 않는다.
- path traversal과 symlink 관련 위험을 검토한다.
- subject 허용 함수 목록에 `unlink()`가 명시되어 있지 않다. C++98 표준 라이브러리의 `std::remove()` 사용 가능 여부를 실제 캠퍼스 평가 기준과 확인한다.

### status 정책 예시

```text
삭제 성공 → 204 No Content
파일 없음 → 404 Not Found
method 금지 → 405 Method Not Allowed
삭제 권한 없음 → 403 Forbidden
directory 삭제 거절 → 403 또는 409
```

### 완료 조건

- 허용된 파일만 삭제된다.
- 같은 파일을 다시 삭제하면 `404`가 된다.
- traversal 요청으로 root 밖 파일을 삭제할 수 없다.

---

## STEP 14. Transfer-Encoding: chunked 구현

### 구현 작업

- [ ] chunk size line을 16진수로 파싱한다.
- [ ] chunk extension 처리 정책을 정한다.
- [ ] 각 chunk data 뒤의 CRLF를 검증한다.
- [ ] `0` chunk로 body 종료를 판단한다.
- [ ] trailer 처리 정책을 정한다.
- [ ] chunk framing을 제거한 decoded body를 만든다.
- [ ] decoded body 누적 크기에 `client_max_body_size`를 적용한다.
- [ ] CGI에는 unchunked body를 전달한다.

### 반드시 거절할 상황

- [ ] 유효하지 않은 16진수 chunk size.
- [ ] chunk data 길이 부족.
- [ ] CRLF 누락.
- [ ] Content-Length와 chunked framing 충돌.
- [ ] decoded body 제한 초과.

### 완료 조건

- 작은 chunk 여러 개로 보낸 body를 올바르게 합친다.
- framing이 매우 커도 decoded body 기준 제한을 정확히 적용한다.
- malformed chunk 요청 후에도 서버가 계속 동작한다.

---

## STEP 15. CGI 구현

### 15.1 실행 판단

- [ ] 선택된 location에서 CGI가 허용되는지 확인한다.
- [ ] 요청 파일 확장자에 맞는 interpreter를 찾는다.
- [ ] script가 존재하고 실행 대상으로 허용되는 경로인지 확인한다.
- [ ] CGI가 아니면 정적 파일로 처리할지 거절할지 정책을 정한다.

### 15.2 프로세스와 pipe

- [ ] stdin 전달용 pipe를 만든다.
- [ ] stdout 수집용 pipe를 만든다.
- [ ] 필요한 pipe fd를 non-blocking으로 설정한다.
- [ ] `fork()`한다.
- [ ] child에서 `dup2()`로 stdin/stdout을 연결한다.
- [ ] child에서 불필요한 fd를 모두 닫는다.
- [ ] script의 올바른 작업 디렉토리로 `chdir()`한다.
- [ ] `execve()`로 interpreter와 script를 실행한다.
- [ ] parent는 CGI stdin/stdout fd를 같은 poll loop에 등록한다.

### 15.3 CGI 환경변수

최소한 다음 정보를 전달한다.

```text
REQUEST_METHOD
SCRIPT_NAME
SCRIPT_FILENAME
QUERY_STRING
CONTENT_LENGTH
CONTENT_TYPE
SERVER_PROTOCOL
PATH_INFO 정책
일반 request headers를 HTTP_* 형식으로 변환한 값
```

CGI가 client의 full request와 arguments를 사용할 수 있어야 한다.

### 15.4 CGI stdin

- [ ] POST body를 partial write로 전달한다.
- [ ] body를 모두 쓴 뒤 stdin pipe를 닫아 EOF를 보낸다.
- [ ] chunked request는 unchunked body를 전달한다.
- [ ] client disconnect 시 CGI와 pipe를 정리한다.

### 15.5 CGI stdout

- [ ] stdout을 partial read로 계속 수집한다.
- [ ] CGI header와 body를 분리한다.
- [ ] CGI가 제공한 `Content-Type`, `Status` 등을 해석한다.
- [ ] CGI output에 Content-Length가 없으면 stdout EOF로 종료를 판단한다.
- [ ] CGI 결과를 정상 HTTP response로 변환한다.

### 15.6 종료와 timeout

- [ ] `waitpid(..., WNOHANG)`으로 child 종료를 확인한다.
- [ ] CGI timeout을 둔다.
- [ ] timeout이면 `kill()` 후 child를 회수한다.
- [ ] pipe와 pid를 모든 성공/실패 경로에서 정리한다.
- [ ] CGI 실패는 서버 전체 종료가 아니라 `502`/`504` 응답으로 처리한다.

### 완료 조건

- GET query string이 CGI에 전달된다.
- POST body가 CGI stdin에 전달된다.
- CGI가 실행 중이어도 다른 client 요청이 처리된다.
- CGI가 무한 대기해도 timeout 후 서버가 계속 동작한다.
- CGI output에 Content-Length가 없어도 EOF로 완료된다.

---

## STEP 16. 안정성, timeout, 자원 정리 강화

### 구현 작업

- [ ] client idle/request timeout을 구현한다.
- [ ] CGI timeout을 구현한다.
- [ ] client disconnect와 CGI 연결 관계를 정리한다.
- [ ] 모든 close 경로를 점검한다.
- [ ] child process가 zombie로 남지 않게 한다.
- [ ] 무한히 커지는 input/output/CGI buffer를 제한한다.
- [ ] 지나치게 많은 동시 연결에 대한 실패 정책을 정한다.
- [ ] 예외가 event loop 밖으로 전파되어 서버를 죽이지 않게 한다.
- [ ] allocation 실패 등 자원 부족이 전체 서버 crash로 이어지지 않게 한다.
- [ ] 종료 signal에서 listen/client/pipe/child를 정리한다.

### fd 정리 체크

각 fd는 다음 상태를 일관되게 유지해야 한다.

```text
open 여부
non-blocking 여부
poll 등록 여부
소유 객체
close 시 poll/map 제거 여부
fork 후 parent/child 중 누가 닫는지
```

### 완료 조건

- malformed request를 반복해도 서버가 죽지 않는다.
- 연결을 중간에 끊어도 fd가 계속 증가하지 않는다.
- CGI timeout 이후 zombie/pipe leak이 없다.
- 장시간 stress test 후에도 메모리와 fd 사용량이 지속 증가하지 않는다.

---

## STEP 17. 테스트 자산과 데모 사이트 만들기

subject는 모든 기능을 시연할 config와 기본 파일을 요구한다.

### 데모 파일

- [ ] 완전한 정적 사이트: HTML, CSS, 이미지, 선택적으로 JS.
- [ ] custom `404.html`, `500.html`.
- [ ] upload form.
- [ ] upload 결과를 확인할 directory.
- [ ] DELETE 시연용 파일.
- [ ] autoindex 시연용 directory.
- [ ] redirect 시연 route.
- [ ] Python 또는 PHP CGI.
- [ ] CGI GET query 예제.
- [ ] CGI POST body 예제.
- [ ] 두 번째 포트의 다른 사이트.

### 자동 테스트

- [ ] build/relink 검사.
- [ ] config valid/invalid 검사.
- [ ] 여러 포트 검사.
- [ ] GET static/binary 검사.
- [ ] 400/403/404/405/413/5xx 검사.
- [ ] redirect와 `Location` header 검사.
- [ ] index/autoindex 검사.
- [ ] Content-Length body 검사.
- [ ] multipart upload와 파일 byte 비교.
- [ ] DELETE 검사.
- [ ] chunked request 검사.
- [ ] CGI GET/POST/timeout/non-blocking 검사.
- [ ] slow client와 disconnect 검사.
- [ ] 동시 요청 stress 검사.

### 완료 조건

`tests/integration.sh` 같은 한 명령으로 주요 mandatory 기능을 반복 검증할 수 있다.

---

## STEP 18. README와 평가 준비

### README 수정

현재 `README.md`는 영어 형식과 섹션은 갖추고 있지만, 존재하지 않는 구현 경로와 테스트 결과를 설명한다. 구현 완료 후 실제 동작 기준으로 수정한다.

- [ ] 첫 줄의 login 정보가 정확하다.
- [ ] 실제 build/run 명령을 적는다.
- [ ] 실제 config 문법을 설명한다.
- [ ] 실제 지원 기능만 적는다.
- [ ] 테스트 명령과 기대 동작을 적는다.
- [ ] 사용한 참고 자료를 적는다.
- [ ] AI 사용 범위와 검증 방법을 적는다.

### 평가 설명 준비

다음 질문에 코드를 가리키며 설명할 수 있어야 한다.

- [ ] 왜 non-blocking이어야 하는가?
- [ ] 왜 하나의 poll에서 read/write를 같이 감시하는가?
- [ ] listen/client/CGI fd를 어떻게 구분하는가?
- [ ] partial recv/send를 어떻게 처리하는가?
- [ ] client별 buffer와 상태는 무엇인가?
- [ ] 요청 완성을 어떻게 판단하는가?
- [ ] Content-Length와 chunked를 어떻게 처리하는가?
- [ ] server/location 선택과 longest-prefix match는 어떻게 동작하는가?
- [ ] URI를 실제 경로로 어떻게 바꾸며 traversal을 어떻게 막는가?
- [ ] upload boundary와 filename을 어떻게 처리하는가?
- [ ] CGI pipe, env, EOF, timeout을 어떻게 처리하는가?
- [ ] disconnect와 모든 fd를 어떻게 정리하는가?
- [ ] 각 status code를 왜 선택했는가?

평가 중 작은 수정 요청이 있을 수 있으므로, 핵심 구조를 직접 이해하고 빠르게 변경할 수 있어야 한다.

---

## 9. 요청 하나의 최종 처리 순서

모든 기능이 연결된 뒤 요청 하나는 다음 순서로 처리한다.

```text
1. listen fd에서 accept
2. client fd를 non-blocking으로 설정하고 poll 등록
3. POLLIN에서 요청 bytes를 client input buffer에 누적
4. request line/header 파싱
5. 들어온 listen fd 기준 server 선택
6. URI 기준 longest-prefix location 선택
7. effective config 계산
8. body framing과 body limit 결정
9. body 완성까지 계속 수신
10. redirect 검사
11. method 허용 검사
12. CGI/upload/static/DELETE handler 선택
13. 안전한 실제 경로 계산
14. handler 실행 또는 CGI 작업 등록
15. 정상/custom/default error response 생성
16. client를 POLLOUT 감시 상태로 변경
17. partial send와 offset 갱신
18. 전송 완료 후 close 또는 다음 요청 대기
```

성능과 방어를 위해 header만으로 결정 가능한 `413`, `400` 등은 가능한 일찍 처리하되, 실제 처리 순서는 일관된 상태 머신으로 관리한다.

---

## 10. 보안 및 경로 처리 체크리스트

### path traversal

- [ ] URL decode 전후의 path를 검증한다.
- [ ] `..` segment를 정규화하거나 거절한다.
- [ ] 중복 slash와 `.` segment 정책을 정한다.
- [ ] query string을 filesystem path에 포함하지 않는다.
- [ ] root와 결합 후 root 밖 접근이 불가능한지 검증한다.
- [ ] GET, upload, DELETE, CGI 모두 같은 안전한 path helper를 사용한다.

### 파일과 directory

- [ ] regular file, directory, 존재하지 않음, 권한 없음 상태를 구분한다.
- [ ] directory를 일반 파일처럼 읽거나 삭제하지 않는다.
- [ ] autoindex HTML에서 filename을 escape한다.
- [ ] upload filename을 신뢰하지 않는다.
- [ ] CGI는 설정된 route와 extension에 해당하는 script만 실행한다.

### 자원 제한

- [ ] header 최대 크기.
- [ ] URI 최대 크기.
- [ ] decoded body 최대 크기.
- [ ] CGI output 최대 크기 또는 정책.
- [ ] client/CGI timeout.
- [ ] 동시 연결 수 실패 처리.

---

## 11. 테스트 매트릭스

### 11.1 기본 실행

```bash
make re
make
./webserv config/default.conf
```

확인:

- [ ] 두 번째 `make`가 relink하지 않는다.
- [ ] 잘못된 인자 개수에서 오류가 난다.
- [ ] 존재하지 않는 config에서 오류가 난다.
- [ ] invalid config에서 listen 전에 종료한다.

### 11.2 정적 파일과 route

```bash
curl -i http://127.0.0.1:8080/
curl -i http://127.0.0.1:8081/
curl -i http://127.0.0.1:8080/index.html
curl -i http://127.0.0.1:8080/not-found
curl -i http://127.0.0.1:8080/old
curl -i http://127.0.0.1:8080/listing/
```

### 11.3 method, body, upload, DELETE

```bash
curl -i -X POST --data-binary 'hello' http://127.0.0.1:8080/echo
curl -i -F file=@README.md http://127.0.0.1:8080/upload
curl -i http://127.0.0.1:8080/uploads/
curl -i -X DELETE http://127.0.0.1:8080/delete/README.md
curl -i -X PUT http://127.0.0.1:8080/
```

확인:

- [ ] upload 파일 hash/byte가 원본과 같다.
- [ ] overwrite status가 신규 생성 status와 구분된다.
- [ ] body 제한 초과가 `413`이다.
- [ ] route에서 금지된 method와 미구현 method의 응답이 구분된다.

### 11.4 CGI

```bash
curl -i 'http://127.0.0.1:8080/cgi-bin/hello.py?name=test'
curl -i -X POST -d 'name=test' http://127.0.0.1:8080/cgi-bin/hello.py
```

확인:

- [ ] query와 body가 CGI에 전달된다.
- [ ] CGI 실행 중 다른 GET이 즉시 처리된다.
- [ ] 무한 실행 CGI가 timeout된다.
- [ ] 실패 CGI 뒤에도 서버가 동작한다.

### 11.5 raw HTTP와 malformed request

```bash
printf 'GET / HTTP/1.1\r\nHost: localhost\r\n\r\n' | nc 127.0.0.1 8080
printf 'GET / HTTP/1.1\r\n\r\n' | nc 127.0.0.1 8080
printf 'BROKEN\r\n\r\n' | nc 127.0.0.1 8080
```

추가 Python 테스트:

```text
header를 한 byte씩 전송
body를 천천히 전송
요청 중간 disconnect
잘못된 Content-Length
잘못된 chunk size
chunked decoded body limit 경계값
응답을 매우 천천히 읽는 client
```

### 11.6 NGINX 비교

다음 동작을 NGINX와 비교한다.

```text
404와 405 응답
redirect와 Location
directory/index/autoindex
Content-Length
Content-Type
Connection
method별 status
```

HTTP version과 설정 차이로 완전히 같은 raw 응답일 필요는 없지만, status와 핵심 header/행동이 합리적이어야 한다.

### 11.7 stress와 leak

- [ ] 동시에 수백 개 GET 요청.
- [ ] 느린 client를 열어둔 상태의 정상 요청.
- [ ] 큰 파일 download/upload.
- [ ] malformed request 반복.
- [ ] client disconnect 반복.
- [ ] CGI timeout 반복.
- [ ] `valgrind --track-fds=yes`로 fd와 메모리 점검.

---

## 12. 흔한 실패 원인

### 서버가 멈추는 경우

```text
blocking recv/send
CGI pipe blocking
waitpid를 blocking으로 호출
요청/응답 전체가 한 번에 처리된다고 가정
timeout 없음
```

### 브라우저가 계속 로딩하는 경우

```text
Content-Length 불일치
\r\n\r\n 누락
response partial send 누락
Connection 처리 오류
CGI EOF 처리 오류
```

### 업로드 파일이 깨지는 경우

```text
body 완성 전에 저장
multipart boundary 범위 계산 오류
binary 데이터에 C 문자열 함수 사용
filename/path 검증 누락
```

### CGI가 멈추는 경우

```text
body 전송 후 stdin pipe를 닫지 않음
stdout을 blocking read
CGI pipe를 poll에 등록하지 않음
timeout/kill/waitpid 누락
fork 후 불필요한 fd를 닫지 않음
```

### fd 또는 메모리가 증가하는 경우

```text
close 후 poll/map 제거 누락
disconnect 시 CGI 정리 누락
child process 회수 누락
input/output buffer 제한 없음
keep-alive 상태 초기화 누락
```

---

## 13. Bonus는 Mandatory 이후에 구현

Bonus는 mandatory가 완전히 정상일 때만 평가된다.

### Cookie와 session

- [ ] cookie parsing과 `Set-Cookie`.
- [ ] session id 생성.
- [ ] session data 저장.
- [ ] last access time과 TTL.
- [ ] 최대 session 수 또는 정리 정책.
- [ ] 모든 요청에서 불필요하게 session을 생성하지 않음.

### Multiple CGI types

- [ ] `.py`, `.php` 등 extension별 interpreter.
- [ ] interpreter별 실행/실패 테스트.

Mandatory의 chunked body limit, upload status, fd leak, timeout 같은 문제보다 bonus를 먼저 구현하지 않는다.

---

## 14. 최종 제출 체크리스트

### 빌드와 형식

- [ ] `webserv` 이름.
- [ ] C++98 및 필수 compiler flags.
- [ ] 필수 Makefile rules.
- [ ] 불필요한 relink 없음.
- [ ] 외부 라이브러리 없음.
- [ ] 영어 README 요구사항 충족.
- [ ] repository에 필요한 config/default/demo/test 파일 포함.

### 이벤트 루프

- [ ] 하나의 poll/equivalent 흐름.
- [ ] listen 포함.
- [ ] read/write 동시 감시.
- [ ] socket/pipe I/O 전 readiness 확인.
- [ ] partial read/write.
- [ ] client disconnect.
- [ ] timeout.
- [ ] CGI non-blocking.

### HTTP와 기능

- [ ] browser-compatible.
- [ ] GET/POST/DELETE.
- [ ] static website.
- [ ] upload.
- [ ] accurate status.
- [ ] default/custom error page.
- [ ] multiple ports.
- [ ] Content-Length.
- [ ] chunked unchunk.

### Config

- [ ] interface:port.
- [ ] error page.
- [ ] max body size.
- [ ] accepted methods.
- [ ] redirect.
- [ ] route root.
- [ ] autoindex.
- [ ] index.
- [ ] upload/store.
- [ ] CGI extension.

### CGI

- [ ] full request arguments/env 제공.
- [ ] unchunked body와 stdin EOF.
- [ ] stdout EOF 처리.
- [ ] 올바른 working directory.
- [ ] 최소 한 종류 CGI.
- [ ] timeout과 child 회수.

### 안정성

- [ ] malformed request 후 정상 동작.
- [ ] slow client 후 정상 동작.
- [ ] disconnect 후 정상 동작.
- [ ] CGI failure/timeout 후 정상 동작.
- [ ] 자원 부족 및 allocation 실패 대응 경로 검토.
- [ ] stress test 후 정상 동작.
- [ ] memory/fd/child leak 없음.

---

## 15. 가장 현실적인 구현 우선순위

작업량이 많기 때문에 다음 순서를 유지하는 것이 좋다.

```text
1. Makefile과 프로젝트 골격
2. 최소 TCP 서버
3. 하나의 poll 이벤트 루프
4. Client buffer와 상태 머신
5. HTTP request parser
6. HTTP response와 공통 error 처리
7. config parser
8. multiple ports와 Router
9. static GET
10. index와 autoindex
11. Content-Length body와 size limit
12. upload
13. DELETE
14. chunked request
15. CGI
16. timeout, disconnect, resource cleanup
17. integration/stress/leak test
18. README와 평가 준비
19. Mandatory 완성 후 bonus
```

각 단계에서 반드시 자동 또는 반복 가능한 테스트를 함께 추가한다. 마지막에 한꺼번에 테스트를 만들면 어느 계층에서 문제가 생겼는지 찾기 어려워진다.
