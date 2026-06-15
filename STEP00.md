# STEP 00 - 요구사항 분석과 설계 결정

## 이 단계의 목표

코드를 작성하기 전에 Webserv의 범위와 구현 규칙을 확정한다.

이 단계를 생략하면 팀원마다 config 문법, path 결합 방식, 상태 코드 기준이 달라져 이후 모듈을 합칠 때 큰 수정이 필요해진다.

## 완료 후 만들어져 있어야 하는 결과

- 구현할 Mandatory 기능 목록
- 사용하지 않을 기능과 Bonus 범위
- config 문법 예시
- 요청 처리 순서
- Client와 Request Parser 상태 머신
- URI를 실제 파일 경로로 변환하는 규칙
- 주요 HTTP status code 정책
- 팀원별 담당 범위

## 사용자가 해야 할 일

### 1. 공식 명세 확인

- [ ] `en.subject.pdf`의 General rules를 읽는다.
- [ ] Mandatory requirements를 읽는다.
- [ ] Configuration file 요구사항을 읽는다.
- [ ] README 요구사항을 읽는다.
- [ ] Bonus는 Mandatory 완료 후에만 진행한다고 합의한다.
- [ ] 허용 함수 목록을 별도로 기록한다.

반드시 기억할 규칙:

```text
C++98로 구현한다.
외부 라이브러리와 Boost를 사용하지 않는다.
fork는 CGI에만 사용한다.
listen socket을 포함한 모든 socket/pipe I/O는 하나의 poll 흐름으로 관리한다.
read/write/send/recv 전에 readiness event를 확인한다.
서버는 client 오류 때문에 종료되면 안 된다.
```

### 2. 전체 요청 처리 흐름 작성

다음 흐름을 팀원 모두가 설명할 수 있어야 한다.

```text
config 읽기
→ listen socket 생성
→ poll event loop
→ client accept
→ request bytes 누적
→ request 완성 여부 판단
→ request parsing
→ server/location 선택
→ method/body/config 검사
→ static/upload/DELETE/CGI 처리
→ response 생성
→ partial send
→ close 또는 다음 요청
```

- [ ] 각 단계의 담당 클래스 또는 함수를 결정한다.
- [ ] 한 client의 오류가 발생했을 때 어디에서 response를 만드는지 결정한다.
- [ ] client와 CGI의 관계를 어떻게 저장할지 결정한다.

### 3. config 문법 결정

최소한 다음 directive를 지원하는 문법을 결정한다.

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

예시:

```conf
server {
    listen 127.0.0.1:8080;
    root ./www;
    index index.html;
    client_max_body_size 10M;

    location /upload {
        methods GET POST DELETE;
        upload on;
        upload_store ./www/uploads;
    }
}
```

- [ ] directive별 허용 인자 개수를 정한다.
- [ ] directive 중복 허용 여부를 정한다.
- [ ] server 설정을 location이 상속하는 규칙을 정한다.
- [ ] 잘못된 config 발견 시 서버를 시작하지 않는다고 결정한다.

### 4. URI와 파일 경로 결합 규칙 결정

권장 규칙:

```text
요청 URI: /images/icons/home.png
location: /images
location root: ./www/images
결과 경로: ./www/images/icons/home.png
```

- [ ] query string은 파일 경로에서 제외한다.
- [ ] `..`, `.`, 중복 slash 처리 규칙을 정한다.
- [ ] URL decode 시점과 decode 후 재검사 규칙을 정한다.
- [ ] root 밖 경로 접근을 거절한다고 결정한다.
- [ ] GET, upload, DELETE, CGI가 같은 path helper를 사용하도록 결정한다.

### 5. 상태 머신 결정

Client 상태 예시:

```text
READING_HEADERS
READING_BODY
PROCESSING_REQUEST
RUNNING_CGI
WRITING_RESPONSE
CLOSING
```

Request Parser 상태 예시:

```text
REQUEST_LINE
HEADERS
BODY_BY_LENGTH
BODY_CHUNK_SIZE
BODY_CHUNK_DATA
BODY_CHUNK_CRLF
COMPLETE
ERROR
```

- [ ] 각 상태에서 허용되는 작업을 정한다.
- [ ] 상태가 변경되는 조건을 정한다.
- [ ] 에러가 발생했을 때 이동할 상태를 정한다.

### 6. status code 정책 결정

최소 정책:

| 상황 | Status |
|---|---:|
| 정상 GET | `200 OK` |
| 새 파일 업로드 | `201 Created` |
| DELETE 성공 | `204 No Content` |
| 잘못된 request | `400 Bad Request` |
| 접근 금지 | `403 Forbidden` |
| 리소스 없음 | `404 Not Found` |
| route에서 method 금지 | `405 Method Not Allowed` |
| body 제한 초과 | `413 Payload Too Large` |
| 미구현 method | `501 Not Implemented` |
| CGI 실패 | `502 Bad Gateway` |
| CGI timeout | `504 Gateway Timeout` |
| 지원하지 않는 HTTP version | `505 HTTP Version Not Supported` |

- [ ] `405`와 `501`의 차이를 팀원이 설명할 수 있다.
- [ ] custom error page 실패 시 기본 error page를 사용한다고 결정한다.

## 이 단계에서 구현하지 않을 것

- socket 코드
- HTTP parser 코드
- CGI 코드
- Bonus 기능

이 단계는 실제 코드를 만드는 단계가 아니라, 이후 코드를 다시 뜯어고치지 않도록 기준을 고정하는 단계이다.

## 확인 질문

- 왜 socket과 CGI pipe를 같은 poll 흐름에서 처리해야 하는가?
- request가 recv 한 번에 전부 도착하지 않으면 어떻게 처리할 것인가?
- location이 여러 개 일치하면 무엇을 선택할 것인가?
- `/../../etc/passwd` 요청을 어떻게 막을 것인가?
- route에서 금지된 `DELETE`와 서버가 구현하지 않은 `PUT`을 어떻게 구분할 것인가?

## 완료 조건

- [ ] config 예시와 각 directive 의미가 정해졌다.
- [ ] request 처리 흐름을 팀원 모두 설명할 수 있다.
- [ ] Client와 Parser 상태 머신이 정해졌다.
- [ ] URI/path 결합 규칙이 정해졌다.
- [ ] 주요 status code 기준이 정해졌다.
- [ ] 담당 모듈과 공통 인터페이스가 정해졌다.

다음 단계: [STEP01.md](STEP01.md)
