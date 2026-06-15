# STEP 19 - README와 평가 준비

## 이 단계의 목표

실제 구현과 문서를 일치시키고, 평가 중 모든 핵심 구조를 직접 설명하고 작은 수정 요청을 수행할 수 있게 한다.

## 선행 조건

- [ ] [STEP18.md](STEP18.md)의 통합 및 stress 테스트가 통과한다.
- [ ] Mandatory 기능이 모두 구현되었다.
- [ ] 평가용 config와 demo 파일이 준비되었다.

## 현재 문서에서 특히 확인할 점

기존 `README.md`와 `TEST.md`에는 현재 구현이 존재한다고 가정한 설명이 포함되어 있었다. 실제 구현 완료 후에는 반드시 실제 파일, 기능, 테스트 결과와 일치하게 수정해야 한다.

## 사용자가 해야 할 일

### 1. 영어 README 형식 검사

첫 줄은 italic 형식으로 다음 내용을 포함해야 한다.

```text
This project has been created as part of the 42 curriculum by <login>.
```

- [ ] 첫 줄 형식과 login을 확인한다.
- [ ] README가 repository root에 있는지 확인한다.
- [ ] README가 영어로 작성되었는지 확인한다.

### 2. 필수 README section 작성

#### Description

- [ ] 프로젝트 목표를 설명한다.
- [ ] C++98 HTTP 서버임을 설명한다.
- [ ] 주요 기능을 실제 구현 기준으로 설명한다.

#### Instructions

- [ ] build 명령을 적는다.
- [ ] 실행 명령과 config 경로를 적는다.
- [ ] 브라우저/curl 테스트 방법을 적는다.
- [ ] 환경 요구사항을 적는다.

#### Resources

- [ ] HTTP/CGI/NGINX/man page 참고 자료를 적는다.
- [ ] AI 사용 작업을 구체적으로 적는다.
- [ ] AI 결과를 어떻게 검토하고 테스트했는지 적는다.

### 3. 실제 구현과 README 대조

- [ ] README의 directory 구조가 실제와 같은지 확인한다.
- [ ] 지원한다고 적은 기능이 실제 테스트를 통과하는지 확인한다.
- [ ] 존재하지 않는 config/test 경로를 제거한다.
- [ ] 실행 명령을 그대로 복사해 실행해본다.
- [ ] 알려진 제한을 숨기지 않고 정확히 적는다.

### 4. 평가용 데모 준비

평가용 config 하나로 다음을 보여줄 수 있게 한다.

- [ ] 서로 다른 콘텐츠를 제공하는 여러 port.
- [ ] 정적 HTML/CSS/이미지.
- [ ] custom error page.
- [ ] redirect.
- [ ] method 제한.
- [ ] index와 autoindex.
- [ ] upload form과 저장 결과.
- [ ] DELETE.
- [ ] CGI GET/POST.
- [ ] body limit.
- [ ] chunked request 테스트.

### 5. 전체 구조 설명 연습

다음 흐름을 코드 위치와 함께 설명한다.

```text
main
→ config parsing
→ listen socket 생성
→ poll event loop
→ accept
→ client buffer와 parser
→ routing/effective config
→ handler 또는 CGI
→ response builder
→ partial send
→ cleanup
```

### 6. 예상 질문 준비

#### non-blocking

- 왜 non-blocking이어야 하는가?
- 왜 poll을 하나만 사용하는가?
- read/write 전에 readiness를 확인하는 이유는?
- partial recv/send를 어떻게 처리하는가?

#### HTTP

- request 완료를 어떻게 판단하는가?
- Host, Content-Length, chunked를 어떻게 처리하는가?
- `405`와 `501` 차이는?
- Content-Length를 어떻게 계산하는가?

#### Config와 path

- config tokenizer/parser 구조는?
- server/location 상속은?
- longest-prefix match는?
- traversal을 어떻게 막는가?

#### Upload와 DELETE

- multipart boundary를 어떻게 파싱하는가?
- binary 파일을 어떻게 보존하는가?
- filename을 어떻게 검증하는가?
- DELETE가 root 밖 파일을 지우지 못하게 어떻게 막는가?

#### CGI

- pipe가 왜 두 개인가?
- CGI stdin/stdout과 EOF를 어떻게 처리하는가?
- 환경변수는 무엇을 전달하는가?
- CGI timeout과 child 회수는?

### 7. 짧은 수정 연습

평가 중 작은 변경이 요청될 수 있다.

연습 예:

- [ ] 새로운 MIME type 추가.
- [ ] timeout 값 변경.
- [ ] status page 문구 변경.
- [ ] config directive 기본값 변경.
- [ ] 특정 header 추가.

모든 팀원은 자신이 작성하지 않은 핵심 모듈도 이해해야 한다.

### 8. 최종 제출 파일 검사

- [ ] 실제 Git repository인지 확인한다.
- [ ] 제출할 파일 이름을 확인한다.
- [ ] build artifact와 불필요한 파일을 제출하지 않는지 확인한다.
- [ ] config와 demo/test 파일이 repository에 포함되었는지 확인한다.
- [ ] 깨끗한 환경에서 clone/build/run을 검증한다.

## 평가 직전 실행

```bash
make fclean
make
make
./tests/integration.sh
```

추가:

- 브라우저 시연.
- curl 수동 시연.
- raw socket/chunked 테스트.
- Valgrind 핵심 경로.

## 자주 발생하는 문제

### README에는 기능이 있지만 실제로 실패함

README를 희망 사항이 아니라 실제 통합 테스트 결과에 맞춰 수정한다.

### 본인 담당 코드만 설명 가능함

Webserv 평가는 전체 구조 이해가 중요하다. 팀원끼리 서로의 모듈을 설명하고 질문한다.

### 테스트 명령이 특정 로컬 환경에서만 동작함

상대 경로, interpreter 경로, 권한, 실행 위치를 점검한다.

## 완료 조건

- [ ] README 필수 형식을 만족한다.
- [ ] README의 모든 명령과 기능이 실제로 동작한다.
- [ ] 평가용 config로 모든 Mandatory 기능을 시연할 수 있다.
- [ ] 모든 핵심 구조를 코드와 함께 설명할 수 있다.
- [ ] 짧은 수정 요청에 대응할 수 있다.
- [ ] 깨끗한 환경에서 build/run/test를 검증했다.

다음 단계: [STEP20.md](STEP20.md)
