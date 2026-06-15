# STEP 07 - 정적 파일 GET 구현

## 이 단계의 목표

요청 URI에 해당하는 실제 파일을 읽어 브라우저에 제공한다.

이 단계에서는 config parser 전이므로 초기 root를 `./www`로 고정해도 된다.

## 선행 조건

- [ ] [STEP06.md](STEP06.md)가 완료되었다.
- [ ] GET request의 path를 얻을 수 있다.
- [ ] 공통 ResponseBuilder가 동작한다.

## 만들 파일 예시

```text
include/StaticFileHandler.hpp
include/MimeTypes.hpp
src/handlers/StaticFileHandler.cpp
src/utils/MimeTypes.cpp
www/index.html
www/style.css
www/images/
```

## 사용자가 해야 할 일

### 1. 테스트 정적 사이트 작성

- [ ] `www/index.html`을 만든다.
- [ ] `www/style.css`를 만든다.
- [ ] 이미지 파일 하나를 추가한다.
- [ ] HTML에서 CSS와 이미지를 상대 또는 절대 URI로 요청하게 한다.

브라우저는 HTML을 받은 후 CSS와 이미지에 대해 별도 GET 요청을 보낸다는 점을 확인한다.

### 2. URI를 실제 파일 경로로 변환

초기 규칙:

```text
root: ./www
URI: /style.css
path: ./www/style.css
```

- [ ] query string을 제외한 request path만 사용한다.
- [ ] `/` 요청을 `index.html`로 임시 매핑한다.
- [ ] 경로 결합을 공통 helper로 만든다.
- [ ] `..` path traversal을 거절한다.
- [ ] root 밖 경로에 접근할 수 없게 한다.

### 3. 파일 상태 확인

- [ ] `stat()`으로 존재 여부를 확인한다.
- [ ] regular file인지 확인한다.
- [ ] directory인 경우는 이 단계에서는 별도 처리하거나 `403`으로 응답한다.
- [ ] 접근할 수 없는 파일은 `403`으로 처리한다.
- [ ] 존재하지 않는 파일은 `404`로 처리한다.

### 4. 파일 읽기

- [ ] `open()`으로 파일을 연다.
- [ ] 파일 내용을 binary-safe하게 읽는다.
- [ ] 파일 전체를 읽는 동안 오류를 처리한다.
- [ ] 성공과 실패 경로에서 file fd를 닫는다.
- [ ] 읽은 body를 HttpResponse에 넣는다.

일반 디스크 파일은 subject에서 poll readiness 감시의 예외이다.

### 5. MIME type 결정

최소 매핑:

```text
.html      text/html
.css       text/css
.js        application/javascript
.txt       text/plain
.jpg/.jpeg image/jpeg
.png       image/png
.gif       image/gif
.ico       image/x-icon
기타       application/octet-stream
```

- [ ] 확장자로 MIME type을 찾는 함수를 만든다.
- [ ] 확장자 없는 파일의 fallback을 정한다.
- [ ] response `Content-Type`에 적용한다.

### 6. response 전송

- [ ] 정상 파일은 `200 OK`로 응답한다.
- [ ] body 크기로 Content-Length를 계산한다.
- [ ] 큰 파일도 Client output buffer와 partial send로 보낸다.
- [ ] 에러는 공통 error response를 사용한다.

## 실행 및 검증

```bash
curl -i http://127.0.0.1:8080/
curl -i http://127.0.0.1:8080/index.html
curl -i http://127.0.0.1:8080/style.css
curl -i http://127.0.0.1:8080/images/logo.png
curl -i http://127.0.0.1:8080/not-found
```

binary 파일 비교:

```bash
curl -s http://127.0.0.1:8080/images/logo.png -o /tmp/webserv-logo.png
cmp www/images/logo.png /tmp/webserv-logo.png
```

브라우저 확인:

- HTML 표시.
- CSS 적용.
- 이미지 표시.
- 개발자 도구에서 각 요청 status 확인.

## 보안 테스트

```text
GET /../../etc/passwd
GET /images/../../../etc/passwd
URL encoded traversal
```

모두 root 밖 파일을 제공하지 않아야 한다.

## 자주 발생하는 문제

### HTML은 보이지만 CSS가 적용되지 않음

확인:

- CSS URI 요청을 실제 파일로 찾는가?
- `Content-Type: text/css`인가?

### 이미지가 깨짐

확인:

- C 문자열 함수로 파일 body를 처리하지 않는가?
- 읽은 byte 수 기준으로 append하는가?

### 경로를 문자열 포함 검사만으로 검증함

단순 문자열 검사만으로 traversal을 막기 어렵다. path segment 정규화와 root 내부 여부를 일관된 helper에서 처리한다.

## 완료 조건

- [ ] HTML, CSS, 이미지가 브라우저에서 표시된다.
- [ ] 정적 파일을 binary-safe하게 읽고 전송한다.
- [ ] MIME type이 적용된다.
- [ ] 없는 파일은 `404`, 접근 불가는 `403`이다.
- [ ] traversal로 root 밖 파일을 읽을 수 없다.
- [ ] 큰 파일도 partial send로 전송한다.

다음 단계: [STEP08.md](STEP08.md)
