# STEP 02 - 최소 TCP 서버 만들기

## 이 단계의 목표

`127.0.0.1:8080`에서 연결을 받고, 올바른 고정 HTTP 응답을 보내는 최소 TCP 서버를 만든다.

이 단계에서는 socket의 기본 흐름을 이해하는 데 집중한다.

```text
socket → setsockopt → bind → listen → accept → send → close
```

## 선행 조건

- [ ] [STEP01.md](STEP01.md)가 완료되었다.
- [ ] `make`로 `webserv`가 생성된다.
- [ ] 사용할 host와 port를 `127.0.0.1:8080`으로 정했다.

## 만들 파일 예시

```text
include/Server.hpp
src/core/Server.cpp
```

## 사용자가 해야 할 일

### 1. listen socket 생성

- [ ] `socket(AF_INET, SOCK_STREAM, 0)`으로 TCP socket을 만든다.
- [ ] 실패 시 오류를 출력하고 초기화를 중단한다.
- [ ] 생성된 fd의 소유자를 명확히 정한다.

확인할 점:

```text
socket() 결과가 음수인가?
초기화 실패 시 이미 열린 fd를 닫는가?
```

### 2. socket option 설정

- [ ] `setsockopt()`로 주소 재사용 option을 설정한다.
- [ ] 설정 실패 시 socket을 닫고 종료한다.

주소 재사용 설정은 서버 재시작 시 이전 연결 상태 때문에 `bind()`가 실패하는 상황을 줄여준다.

### 3. 주소 구조체 구성과 bind

- [ ] IPv4 주소 구조체를 초기화한다.
- [ ] port를 `htons()`로 network byte order로 변환한다.
- [ ] `127.0.0.1`에 binding할 값을 구성한다.
- [ ] `bind()` 실패를 처리한다.

주의:

```text
host byte order와 network byte order를 구분한다.
bind 실패 후 listen을 진행하지 않는다.
```

### 4. listen 시작

- [ ] `listen()`으로 접속 대기 상태를 만든다.
- [ ] backlog 값을 정한다.
- [ ] 실패 시 listen fd를 닫는다.

### 5. client 연결 받기

- [ ] `accept()`로 client fd를 얻는다.
- [ ] listen fd와 client fd의 역할 차이를 이해한다.
- [ ] client fd에만 response를 보낸다.

```text
listen fd: 새로운 연결을 받는 용도
client fd: 특정 client와 데이터를 주고받는 용도
```

### 6. 고정 HTTP response 전송

보낼 응답:

```http
HTTP/1.1 200 OK\r
Content-Type: text/plain\r
Content-Length: 12\r
Connection: close\r
\r
Hello World!
```

- [ ] status line을 작성한다.
- [ ] header 줄바꿈으로 `\r\n`을 사용한다.
- [ ] header와 body 사이에 빈 줄을 넣는다.
- [ ] `Content-Length`가 body byte 수와 같은지 확인한다.
- [ ] client fd에 response를 전송한다.

### 7. fd 정리

- [ ] response 후 client fd를 닫는다.
- [ ] 서버 종료 시 listen fd를 닫는다.
- [ ] 실패 경로에서도 열린 fd를 닫는다.

## 실행 및 검증

서버 실행:

```bash
./webserv
```

다른 터미널:

```bash
curl -i http://127.0.0.1:8080/
printf 'GET / HTTP/1.1\r\nHost: localhost\r\n\r\n' | nc 127.0.0.1 8080
```

브라우저:

```text
http://127.0.0.1:8080/
```

## 기대 결과

```http
HTTP/1.1 200 OK
Content-Type: text/plain
Content-Length: 12
Connection: close

Hello World!
```

## 자주 발생하는 문제

### bind 실패

확인:

- port가 이미 사용 중인지 확인한다.
- 주소와 port 변환이 올바른지 확인한다.
- 이전 실행의 socket이 남은 경우를 고려한다.

### curl이 계속 기다림

확인:

- `Content-Length`가 정확한가?
- header 끝에 `\r\n\r\n`이 있는가?
- response 후 연결을 닫았는가?

### 브라우저에는 보이지만 HTTP 형식이 틀림

브라우저는 일부 오류를 관대하게 처리할 수 있다. 반드시 `curl -i`와 `nc`로 raw 응답도 확인한다.

## 이 단계의 제한

현재 구현은 blocking일 수 있고 client 하나만 처리할 수 있다. 이는 학습용 임시 상태이다.

다음 단계에서 반드시:

- listen/client fd를 non-blocking으로 설정한다.
- `poll()` event loop로 교체한다.
- readiness 없이 accept/send/recv하지 않게 만든다.

## 완료 조건

- [ ] socket, bind, listen, accept 흐름이 동작한다.
- [ ] curl과 브라우저에서 `Hello World!`가 보인다.
- [ ] raw HTTP response 형식이 올바르다.
- [ ] `Content-Length`가 정확하다.
- [ ] 성공과 실패 경로에서 fd를 닫는다.

다음 단계: [STEP03.md](STEP03.md)
