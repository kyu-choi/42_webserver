# STEP 08 - Config Parser 구현

## 이 단계의 목표

코드에 하드코딩된 port, root, route 동작을 config 파일에서 읽도록 만든다.

서버는 잘못된 config를 발견하면 socket을 열기 전에 명확히 실패해야 한다.

## 선행 조건

- [ ] [STEP07.md](STEP07.md)의 정적 GET이 동작한다.
- [ ] [STEP00.md](STEP00.md)에서 config 문법을 결정했다.

## 만들 파일 예시

```text
include/ConfigTypes.hpp
include/ConfigParser.hpp
src/config/ConfigParser.cpp
config/default.conf
config/invalid/
```

## 필요한 설정 구조

```text
ListenConfig
ServerConfig
LocationConfig
RedirectConfig
CgiConfig
```

ServerConfig 주요 값:

```text
listen 목록
root
index 목록
error page map
client max body size
location 목록
```

LocationConfig 주요 값:

```text
location prefix
허용 method
root override
index override
autoindex
redirect
upload 허용 여부와 저장 경로
CGI extension/interpreter
```

## 사용자가 해야 할 일

### 1. tokenizer 구현

- [ ] `{`, `}`, `;`를 독립 token으로 만든다.
- [ ] 일반 문자열 token을 분리한다.
- [ ] 공백과 줄바꿈을 구분자로 처리한다.
- [ ] 빈 token을 만들지 않는다.
- [ ] comment를 지원한다면 정확한 문법을 정한다.

tokenizer는 문법 의미를 해석하지 않고 token 목록만 만들어야 한다.

### 2. server block 파싱

- [ ] 최상위에서 `server` token을 요구한다.
- [ ] `{`와 `}` 쌍을 검사한다.
- [ ] server directive를 저장한다.
- [ ] 여러 server block을 파싱한다.
- [ ] server block 밖의 잘못된 token을 거절한다.

### 3. location block 파싱

- [ ] `location` 뒤 prefix와 `{`를 검사한다.
- [ ] location 내부 directive를 저장한다.
- [ ] 닫는 `}`를 검사한다.
- [ ] 중첩 location을 허용할지 거절할지 명시한다.

### 4. directive별 파싱

최소 지원:

```text
listen
root
index
client_max_body_size
error_page
methods
return
autoindex
upload
upload_store
cgi
```

각 directive마다:

- [ ] 허용 위치가 server인지 location인지 검사한다.
- [ ] 인자 개수를 검사한다.
- [ ] 마지막 semicolon을 검사한다.
- [ ] 값을 내부 타입으로 변환한다.
- [ ] 중복 사용 정책을 적용한다.

### 5. 값 검증

- [ ] port가 숫자이고 `1..65535`인지 검사한다.
- [ ] host가 유효한 형태인지 검사한다.
- [ ] body size 숫자와 `K`, `M`, `G` 단위를 검사한다.
- [ ] size 변환 overflow를 검사한다.
- [ ] error page status가 유효한 숫자인지 검사한다.
- [ ] method가 허용된 token인지 검사한다.
- [ ] `on`/`off` 값이 정확한지 검사한다.
- [ ] `upload on`인데 store가 없으면 거절한다.
- [ ] CGI extension과 interpreter 인자를 검사한다.
- [ ] 알 수 없는 directive를 거절한다.

### 6. 기본값과 상속 준비

- [ ] server 기본 root/index/body size를 정한다.
- [ ] location에 값이 명시되었는지 나타내는 상태를 저장한다.
- [ ] 빈 값과 미설정 상태를 구분한다.
- [ ] 이후 Router가 effective config를 계산할 수 있게 한다.

### 7. 초기화 순서 연결

```text
main
→ config 경로 결정
→ 파일 읽기
→ tokenize
→ parse
→ 모든 값 검증
→ 성공한 경우에만 Server/listen socket 생성
```

- [ ] config 오류 시 listen socket을 열지 않는다.
- [ ] 오류 위치와 원인을 가능한 명확히 출력한다.

## 작성할 config 테스트

정상:

```text
config/default.conf
config/multiple_ports.conf
```

오류:

```text
config/invalid/missing_semicolon.conf
config/invalid/unclosed_server.conf
config/invalid/bad_port.conf
config/invalid/bad_size.conf
config/invalid/unknown_directive.conf
config/invalid/bad_method.conf
config/invalid/upload_without_store.conf
```

## 실행 및 검증

```bash
./webserv config/default.conf
./webserv config/invalid/missing_semicolon.conf
./webserv config/invalid/bad_port.conf
./webserv config/invalid/unknown_directive.conf
```

정상 config 결과를 debug 출력하여 모든 값이 예상대로 저장되었는지 확인한다.

## 자주 발생하는 문제

### parser가 공백 형식에 따라 다르게 동작함

해결:

- tokenizer에서 문법 기호와 일반 token을 먼저 명확히 분리한다.

### location이 뒤에 선언된 server 값을 상속하지 못함

해결:

- parsing 중 즉시 복사하지 말고, 전체 server parsing 후 effective config를 계산하거나 미설정 상태를 유지한다.

### 잘못된 값이 조용히 기본값으로 변경됨

잘못된 config는 임의로 보정하지 말고 명확히 거절한다.

## 완료 조건

- [ ] 정상 config를 내부 구조로 변환한다.
- [ ] 여러 server와 location을 저장한다.
- [ ] 모든 최소 directive를 파싱한다.
- [ ] 잘못된 문법과 값을 거절한다.
- [ ] config 성공 후에만 socket 초기화를 진행한다.
- [ ] 미설정 값과 override 값을 구분한다.

다음 단계: [STEP09.md](STEP09.md)
