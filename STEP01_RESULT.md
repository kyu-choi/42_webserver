# STEP 01 구현 결과

## 목표

`STEP01.md`의 목표는 실제 웹서버 기능을 만들기 전에 C++98 프로젝트가 안정적으로 빌드되는 기반을 준비하는 것이다.

이번 단계에서는 `socket`, `poll`, HTTP parsing 같은 서버 기능은 아직 구현하지 않고, 다음 단계에서 기능을 안전하게 붙일 수 있는 프로젝트 구조와 빌드 시스템을 만들었다.

## 생성한 파일과 디렉토리

| 경로 | 구현 내용 |
|---|---|
| `.gitignore` | 검증 중 생성되는 `build/`와 `webserv` 산출물 제외 |
| `Makefile` | `webserv` 실행 파일을 만드는 C++98 빌드 시스템 |
| `src/main.cpp` | 인자 개수 검사, config 경로 결정, top-level 예외 처리 |
| `src/handlers/.gitkeep` | handler 디렉토리를 저장소에 유지하기 위한 placeholder |
| `www/.gitkeep` | 정적 파일 root 디렉토리를 저장소에 유지하기 위한 placeholder |
| `tests/.gitkeep` | 테스트 디렉토리를 저장소에 유지하기 위한 placeholder |

이미 `STEP00`에서 생성된 다음 디렉토리와 파일도 `STEP01` 빌드 대상에 포함했다.

```text
include/
src/config/
src/core/
src/http/
src/utils/
config/default.conf
```

## Makefile 구현 내용

`Makefile`은 과제 요구사항에 맞게 다음 기본값을 사용한다.

```makefile
NAME := webserv
CXX := c++
CXXFLAGS := -Wall -Wextra -Werror -std=c++98
```

구현한 rule은 다음과 같다.

| Rule | 동작 |
|---|---|
| `all` | `webserv` 빌드 |
| `$(NAME)` | object 파일들을 링크해서 실행 파일 생성 |
| `clean` | `build/` object 디렉토리 제거 |
| `fclean` | `clean` 후 `webserv` 실행 파일 제거 |
| `re` | 전체 삭제 후 다시 빌드 |

추가로 다음을 처리했다.

- object 파일은 `build/` 아래에 생성한다.
- source tree 구조와 같은 하위 디렉토리를 `build/` 안에 만든다.
- `-MMD -MP`로 header dependency 파일을 생성한다.
- 두 번째 `make`에서 변경된 파일이 없으면 다시 컴파일하거나 relink하지 않는다.
- source 목록을 `SRCS` 변수로 모아 이후 단계에서 파일을 추가하기 쉽게 했다.

## main.cpp 구현 내용

`src/main.cpp`는 아직 서버를 실행하지 않는다. 지금 단계에서는 실행 정책만 고정한다.

지원하는 실행 형태는 다음과 같다.

```text
./webserv
-> config/default.conf 사용

./webserv config/default.conf
-> 전달된 config 경로 사용

./webserv a.conf b.conf
-> Usage 출력 후 1 반환
```

`main()`에서 구현한 정책은 다음과 같다.

- 인자 개수가 0개 또는 1개의 config 경로만 허용되도록 검사한다.
- 인자가 없으면 기본 config 경로를 `config/default.conf`로 결정한다.
- 인자가 하나 있으면 그 경로를 config 경로로 사용한다.
- 인자가 너무 많으면 `Usage: ./webserv [configuration file]` 형식으로 출력한다.
- `std::exception`과 알 수 없는 예외를 top-level에서 잡아 비정상 종료를 막는다.

## 이번 단계에서 아직 하지 않은 것

`STEP01.md` 범위를 넘기지 않기 위해 다음은 구현하지 않았다.

- config 파일을 실제로 열고 파싱하기
- listen socket 생성
- `poll()` event loop
- client accept
- HTTP request parsing
- response 생성
- static file, upload, DELETE, CGI 처리

이 기능들은 `STEP02.md` 이후 단계에서 순서대로 붙인다.

## 완료 체크

- [x] `Makefile`을 만들었다.
- [x] `include/`가 존재한다.
- [x] `src/main.cpp`가 존재한다.
- [x] `src/config/`가 존재한다.
- [x] `src/core/`가 존재한다.
- [x] `src/http/`가 존재한다.
- [x] `src/handlers/`가 존재한다.
- [x] `src/utils/`가 존재한다.
- [x] `config/`가 존재한다.
- [x] `www/`가 존재한다.
- [x] `tests/`가 존재한다.
- [x] `make`, `clean`, `fclean`, `re`를 검증했다.
- [x] C++98 빌드 플래그를 적용했다.
- [x] 잘못된 인자 개수에서 non-zero 종료하도록 구현했다.

다음 단계는 `STEP02.md`의 최소 TCP 서버 구현이다.
