# STEP 01 - 프로젝트 구조와 Makefile 만들기

## 이 단계의 목표

기능 구현을 시작하기 전에 C++98 프로젝트가 안정적으로 빌드되는 기반을 만든다.

## 선행 조건

- [ ] [STEP00.md](STEP00.md)의 설계 결정이 완료되었다.
- [ ] 실행 파일 이름을 `webserv`로 확정했다.
- [ ] 기본 config 경로 정책을 정했다.

## 완료 후 만들어져 있어야 하는 결과

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

## 사용자가 해야 할 일

### 1. 디렉토리 생성

- [ ] `include/`를 만든다.
- [ ] `src/config/`를 만든다.
- [ ] `src/core/`를 만든다.
- [ ] `src/http/`를 만든다.
- [ ] `src/handlers/`를 만든다.
- [ ] `src/utils/`를 만든다.
- [ ] `config/`, `www/`, `tests/`를 만든다.

각 디렉토리의 책임:

| 경로 | 역할 |
|---|---|
| `include/` | 공개 header |
| `src/config/` | config tokenizer/parser |
| `src/core/` | server, event loop, client |
| `src/http/` | request, response, router |
| `src/handlers/` | static, upload, DELETE, CGI |
| `src/utils/` | string, path, MIME 등 공통 기능 |
| `config/` | 실행 및 평가용 config |
| `www/` | 정적 사이트와 CGI 파일 |
| `tests/` | 자동 및 수동 테스트 |

### 2. 최소 main 작성

`src/main.cpp`에서 해야 할 일:

- [ ] 인자 개수를 검사한다.
- [ ] config 경로를 결정한다.
- [ ] 잘못된 인자 개수에서 사용법을 출력한다.
- [ ] 예상하지 못한 예외가 전체 프로그램을 비정상 종료시키지 않도록 top-level 정책을 정한다.
- [ ] 아직 Server가 없으므로 임시 성공 메시지 또는 종료 코드만 둔다.

권장 실행 정책:

```text
./webserv                 → config/default.conf 사용
./webserv path.conf       → 전달된 config 사용
./webserv a.conf b.conf   → 오류 출력 후 non-zero 종료
```

### 3. Makefile 작성

필수 설정:

```makefile
NAME := webserv
CXX := c++
CXXFLAGS := -Wall -Wextra -Werror -std=c++98
```

필수 rule:

- [ ] `$(NAME)`
- [ ] `all`
- [ ] `clean`
- [ ] `fclean`
- [ ] `re`

추가로 해야 할 일:

- [ ] source 목록을 역할별로 관리한다.
- [ ] object 파일을 별도 directory에 생성한다.
- [ ] header dependency를 처리한다.
- [ ] directory가 없어도 object build 전에 생성되게 한다.
- [ ] 두 번째 `make`에서 relink하지 않게 한다.

### 4. C++98 위반 방지

사용하지 말아야 할 예:

```text
auto
nullptr
lambda
range-based for
std::unordered_map
std::filesystem
smart pointer
```

사용 가능한 대표 도구:

```text
std::string
std::vector
std::map
std::set
std::list
std::stringstream
```

- [ ] 모든 신규 코드를 `-std=c++98`로 컴파일한다.
- [ ] 외부 라이브러리와 Boost를 추가하지 않는다.

## 실행 및 검증

```bash
make
make
make clean
make
make fclean
make re
./webserv
./webserv config/default.conf
./webserv a.conf b.conf
```

확인할 결과:

- 첫 번째 `make`: 실행 파일 생성.
- 두 번째 `make`: 불필요한 compile/relink 없음.
- `clean`: object 제거, 실행 파일 유지.
- `fclean`: object와 실행 파일 제거.
- `re`: 전체 재빌드.
- 잘못된 인자: 명확한 오류와 non-zero 종료.

## 자주 발생하는 문제

### 두 번째 make에서 다시 링크함

원인:

- 실행 파일 target의 dependency가 잘못됨.
- 매번 실행되는 phony target에 연결됨.
- object timestamp 관리가 잘못됨.

### header 수정 후 필요한 파일이 재컴파일되지 않음

해결:

- dependency file을 생성하거나 header dependency를 Makefile에 명시한다.

### 폴더 생성 명령 때문에 항상 target이 실행됨

해결:

- directory target을 order-only prerequisite 등으로 관리한다.

## 완료 조건

- [ ] 모든 필수 디렉토리가 존재한다.
- [ ] `src/main.cpp`가 존재한다.
- [ ] `make`, `clean`, `fclean`, `re`가 정상 동작한다.
- [ ] 불필요한 relink가 없다.
- [ ] C++98과 모든 compiler flag를 만족한다.
- [ ] 인자 정책이 정상 동작한다.

다음 단계: [STEP02.md](STEP02.md)
