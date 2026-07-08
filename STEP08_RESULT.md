# STEP 08 구현 결과

## 목표

`STEP08.md`의 목표는 코드에 하드코딩되어 있던 listen 주소, port, root 같은 값을 config 파일에서 읽도록 만드는 것이다.

이번 단계에서는 `ConfigParser`를 추가하고, `main()`의 초기화 순서를 다음처럼 바꿨다.

```text
config 경로 결정
-> config 파일 읽기
-> tokenize
-> parse/validate
-> 성공한 경우에만 Server 생성
-> socket open
```

## 생성/수정한 파일

| 경로 | 구현 내용 |
|---|---|
| `include/webserv/ConfigParser.hpp` | config parser 인터페이스 선언 |
| `src/config/ConfigParser.cpp` | tokenizer, server/location block parser, directive parser 구현 |
| `include/webserv/Config.hpp` | location override 여부를 저장하는 flag 추가 |
| `src/config/Config.cpp` | location 기본값과 override flag 초기화 |
| `include/webserv/Server.hpp` | `ServerConfig` 기반 생성자 추가 |
| `src/core/Server.cpp` | config listen/root를 사용하고 IPv4 host 변환 확장 |
| `include/webserv/EventLoop.hpp` | config root를 보관하도록 생성자 변경 |
| `src/core/EventLoop.cpp` | `StaticFileHandler` root를 config root로 변경 |
| `src/main.cpp` | config parser 실행 후 서버 생성으로 초기화 순서 변경 |
| `Makefile` | `ConfigParser.cpp` 빌드 대상 추가 |
| `config/multiple_ports.conf` | 여러 server block 정상 config 예시 |
| `config/invalid/*.conf` | invalid config 검증 파일 |

## Tokenizer 구현 내용

tokenizer는 config 파일 문자열을 문법 token 목록으로 만든다.

```text
{, }, ;
-> 독립 token

공백, tab, newline
-> token 구분자

# comment
-> 줄 끝까지 무시
```

tokenizer는 의미를 해석하지 않고 token만 만든다. 문법 해석은 parser 단계에서 한다.

## Parser 구현 내용

지원하는 구조는 다음과 같다.

```text
top level
-> server block 여러 개

server block
-> listen, root, index, client_max_body_size, error_page, location

location block
-> root, index, client_max_body_size, error_page, methods, return,
   autoindex, upload, upload_store, cgi
```

중첩 location은 거절한다.

## Directive 검증

현재 검증하는 값은 다음과 같다.

```text
listen
-> IPv4 host, port 1..65535

client_max_body_size
-> 숫자 + optional K/M/G suffix, overflow 검사

error_page
-> status 100..599 + URI/path

methods
-> GET, POST, DELETE만 허용

return
-> 301 또는 302만 허용

autoindex/upload
-> on 또는 off만 허용

upload on
-> upload_store가 없으면 config 오류

cgi
-> extension은 .으로 시작해야 함

unknown directive
-> config 오류
```

중복 directive는 `listen`, `error_page`, `cgi`처럼 명시적으로 여러 번 허용하는 경우를 제외하고 거절한다.

## 기본값과 override 구분

`LocationConfig`에는 다음 flag를 추가했다.

```text
rootSet
indexesSet
methodsSet
autoindexSet
uploadSet
uploadStoreSet
redirectSet
clientMaxBodySizeSet
```

이렇게 해서 빈 문자열과 “아직 설정하지 않음”을 구분할 수 있다. 다음 단계의 router/effective config 계산에서 server 값 상속 여부를 판단할 수 있다.

## Runtime 연결

`main()`은 이제 config parsing이 성공한 뒤에만 `Server`를 만든다.

현재 STEP08에서는 첫 번째 server block의 첫 번째 listen만 실행에 사용한다. 여러 server block은 parsing/storage까지 하고, 실제 multi-port listen은 `STEP09.md`에서 확장한다.

정적 파일 root는 더 이상 `EventLoop` 내부 하드코딩이 아니라 config의 server root를 사용한다.

```text
server.root
-> Server
-> EventLoop
-> StaticFileHandler
```

## 추가한 테스트 config

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

## 검증 결과

빌드:

```bash
make
make
```

첫 번째 `make`는 새 parser를 포함해 전체 빌드가 성공했고, 두 번째 `make`는 `Nothing to be done for 'all'.`로 no-relink를 확인했다.

정상 config:

```bash
./webserv config/default.conf
curl -i http://127.0.0.1:8080/
curl -i http://127.0.0.1:8080/style.css
curl -i http://127.0.0.1:8080/nope
```

확인한 결과:

```text
/          -> 200 OK, ./www/index.html
/style.css -> 200 OK, text/css
/nope      -> 404 Not Found
```

여러 server block config:

```bash
./webserv config/multiple_ports.conf
```

출력으로 `parsed servers: 2`를 확인했다. STEP08 범위에서는 첫 번째 server의 첫 번째 listen만 실제 실행에 사용한다.

오류 config:

```bash
./webserv config/invalid/missing_semicolon.conf
./webserv config/invalid/unclosed_server.conf
./webserv config/invalid/bad_port.conf
./webserv config/invalid/bad_size.conf
./webserv config/invalid/unknown_directive.conf
./webserv config/invalid/bad_method.conf
./webserv config/invalid/upload_without_store.conf
```

모두 exit code 1로 실패했고, socket 생성 전에 config 오류를 출력했다.

## 이번 단계의 한계

- 여러 server block을 저장하지만 실제 실행은 첫 server의 첫 listen만 사용한다.
- location routing은 아직 없다.
- parsed location root/method/redirect/upload/cgi는 저장만 하고 실제 request 처리에는 아직 반영하지 않는다.
- custom error page 파일 로딩은 아직 없다.

## 완료 체크

- [x] 정상 config를 내부 구조로 변환한다.
- [x] 여러 server와 location을 저장한다.
- [x] 최소 directive를 파싱한다.
- [x] 잘못된 문법과 값을 거절한다.
- [x] config 성공 후에만 socket 초기화를 진행한다.
- [x] 미설정 값과 override 값을 구분한다.

다음 단계는 `STEP09.md`의 여러 포트와 Location Routing 구현이다.
