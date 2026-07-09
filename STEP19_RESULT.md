# STEP 19 Result - README and Evaluation Preparation

## 구현 요약

이번 단계에서는 서버 기능을 새로 크게 추가하기보다, 평가자가 실제 구현을 바로 확인할 수 있도록 문서와 평가용 설정을 현재 코드 기준으로 정리했다.

핵심 결과:

- 루트 `README.md`를 subject 요구에 맞는 영어 README로 교체했다.
- `TEST.md`를 현재 STEP18/STEP19 구현과 테스트 기준의 평가 체크리스트로 갱신했다.
- 평가 시연용 config인 `config/step19.conf`를 추가했다.
- 서버 시작 로그에서 오래된 STEP17 문구를 제거했다.
- 자동 config 테스트가 `config/step19.conf`도 검증하도록 연결했다.

## 수정/추가된 파일

| 파일 | 내용 |
| --- | --- |
| `README.md` | 영어 공식 README로 정리 |
| `TEST.md` | 현재 구현 기준 평가 체크리스트로 갱신 |
| `config/step19.conf` | 평가 시연용 통합 config 추가 |
| `src/main.cpp` | 시작 로그의 오래된 STEP17 문구 제거 |
| `tests/config_suite.py` | step19 config 시작 검증 추가 |
| `STEP19_RESULT.md` | 이번 단계 구현 결과 문서 |

## README.md

첫 줄을 subject 요구 형식에 맞췄다.

```md
*This project has been created as part of the 42 curriculum by kyu-choi.*
```

그리고 README 전체를 영어로 작성했다.

포함한 주요 section:

- `Description`
- `Instructions`
- `Configuration`
- `Project Structure`
- `Request Flow`
- `Resources`

README에는 실제 구현된 기능만 적었다.

명시한 구현 기능:

- C++98 build
- `webserv` 실행 파일
- config 파일 인자
- `poll()` 기반 단일 event loop
- non-blocking listen/client/CGI pipe
- multi-port server
- 정적 파일 serving
- GET/POST/DELETE
- method 제한과 `405 Allow`
- `501 Not Implemented`
- custom error page
- redirect
- index와 autoindex
- upload
- DELETE
- body limit
- chunked request body decode
- Python CGI
- graceful shutdown
- integration/stress/Valgrind smoke test

숨기면 안 되는 제한도 README에 적었다.

- HTTPS/TLS 미지원
- IPv4 숫자 listen host만 지원
- Host header 기반 virtual host 미지원
- cookie/session bonus는 아직 mandatory 구현에 포함하지 않음

## TEST.md

기존 TEST.md에는 이전 상태의 감사 결과가 섞여 있었다.

예전 문서와 현재 구현이 달랐던 대표 항목:

- chunked body limit이 아직 부정확하다고 적혀 있었음
- upload overwrite가 항상 `201`이라고 적혀 있었음
- bonus session/PHP CGI처럼 현재 단계와 맞지 않는 내용이 포함되어 있었음

현재 코드 기준으로 다시 정리했다.

새 TEST.md는 다음 항목을 체크한다.

- 제출 형식과 build
- mandatory runtime feature
- HTTP 동작
- config parser
- static/routing
- upload/delete
- CGI
- 테스트 명령
- 평가 설명용 코드 위치
- 짧은 수정 연습 항목
- bonus status

## config/step19.conf

평가 시연 하나로 mandatory 기능을 보여줄 수 있게 만든 config다.

포함된 기능:

- `127.0.0.1:8080`
- `127.0.0.1:8081`
- 서로 다른 port에서 서로 다른 root 제공
- custom `404`, `413` error page
- redirect `/old`
- method 제한 `/post-only`
- autoindex `/listing`
- index 우선순위 `/has-index`
- body limit `/echo`
- upload `/upload`
- DELETE `/delete`
- Python CGI `/cgi-bin`

실행:

```sh
./webserv config/step19.conf
```

브라우저 확인:

```text
http://127.0.0.1:8080/
http://127.0.0.1:8081/
http://127.0.0.1:8080/listing/
http://127.0.0.1:8080/has-index/
http://127.0.0.1:8080/upload.html
http://127.0.0.1:8080/cgi-bin/hello.py?name=webserv
```

## src/main.cpp

서버 시작 로그가 이전 단계 이름을 계속 출력하고 있었다.

변경 전:

```cpp
std::cout << "webserv STEP17 cleanup-hardened server" << std::endl;
```

변경 후:

```cpp
std::cout << "webserv starting" << std::endl;
```

기능 변화는 없고, 평가 중 혼란을 줄이기 위한 문구 정리다.

## tests/config_suite.py

정상 config 목록에 `config/step19.conf`를 추가했다.

이제 전체 통합 테스트를 실행하면 step19 평가용 config도 실제로 서버가 시작되는지 확인한다.

```py
configs = [
    ("config/default.conf", (8080,)),
    ("config/step18.conf", (8080, 8081)),
    ("config/step19.conf", (8080, 8081)),
    ("config/multiple_ports.conf", (8080, 8081)),
]
```

## 평가 중 설명해야 할 전체 흐름

README와 TEST.md에 다음 흐름을 코드 위치와 함께 정리했다.

```text
main
-> ConfigParser::parseFile
-> Server opens listen sockets
-> EventLoop::run
-> poll
-> accept
-> Client input buffer
-> RequestParser
-> Router and effective location config
-> StaticFileHandler / UploadHandler / DeleteHandler / CgiHandler
-> ResponseBuilder
-> Client output buffer
-> partial send
-> cleanup
```

주요 코드 위치:

- `src/main.cpp`
- `src/config/ConfigParser.cpp`
- `src/core/Server.cpp`
- `src/core/EventLoop.cpp`
- `src/http/RequestParser.cpp`
- `src/http/Router.cpp`
- `src/handlers/StaticFileHandler.cpp`
- `src/handlers/UploadHandler.cpp`
- `src/handlers/DeleteHandler.cpp`
- `src/handlers/CgiHandler.cpp`
- `src/http/ResponseBuilder.cpp`

## 검증 방법

이번 단계 검증은 다음 명령으로 수행한다.

```sh
make
./tests/integration.sh
```

`./tests/integration.sh`는 Valgrind까지 포함하므로 시간이 걸릴 수 있다.

빠른 확인만 할 때는 다음 명령을 사용할 수 있다.

```sh
SKIP_VALGRIND=1 ./tests/integration.sh
```
