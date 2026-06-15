# NGINX 웹 서버 정리

## 1. NGINX란?

NGINX는 **웹 서버(Web Server)** 프로그램 중 하나입니다.  
사용자가 브라우저에서 `https://example.com` 같은 주소로 접속하면, NGINX가 요청을 받아서 **HTML, CSS, JavaScript, 이미지 파일** 등을 응답해줍니다.

NGINX는 보통 **“엔진엑스”** 라고 읽습니다.

---

## 2. NGINX의 주요 역할

NGINX의 핵심 역할은 크게 다음과 같습니다.

1. 정적 파일 제공
2. 리버스 프록시
3. 로드 밸런싱
4. HTTPS 처리
5. 웹 서비스 앞단에서 트래픽 관리

---

## 3. 정적 파일 제공

정적 파일이란 서버에서 바로 전달할 수 있는 파일을 의미합니다.

예를 들어 다음과 같은 웹사이트 파일이 있다고 가정합니다.

```text
/var/www/html
├── index.html
├── style.css
├── app.js
└── logo.png
```

사용자가 브라우저에서 웹사이트에 접속하면 NGINX는 해당 파일들을 찾아서 응답합니다.

```text
브라우저 → "index.html 주세요" → NGINX → index.html 응답
```

즉, 단순한 웹페이지나 이미지, CSS, JavaScript 파일을 빠르게 제공할 수 있습니다.

---

## 4. 리버스 프록시

NGINX는 요청을 직접 처리할 수도 있지만, 뒤에 있는 다른 서버로 요청을 전달할 수도 있습니다.

예를 들어 백엔드 서버가 `localhost:3000`에서 실행 중이라고 가정합니다.

```text
사용자 브라우저
    ↓
NGINX : 80번 포트
    ↓
Node.js / FastAPI / Spring 서버 : 3000번 포트
```

사용자는 `example.com`으로 접속하지만 실제 로그인, 회원가입, 데이터 처리 같은 로직은 백엔드 서버가 처리합니다.

이때 NGINX는 사용자의 요청을 백엔드 서버로 대신 전달하는 역할을 합니다.  
이것을 **리버스 프록시(Reverse Proxy)** 라고 합니다.

---

## 5. 로드 밸런싱

사용자가 많아지면 하나의 서버가 모든 요청을 처리하기 어려워질 수 있습니다.

이때 NGINX는 여러 서버에 요청을 나누어 보낼 수 있습니다.

```text
사용자 요청
   ↓
NGINX
   ├── 서버 1
   ├── 서버 2
   └── 서버 3
```

이처럼 요청을 여러 서버에 분산시키는 기능을 **로드 밸런싱(Load Balancing)** 이라고 합니다.

로드 밸런싱을 사용하면 다음과 같은 장점이 있습니다.

- 서버 한 대에 부하가 몰리는 것을 방지할 수 있음
- 많은 사용자의 요청을 안정적으로 처리할 수 있음
- 일부 서버에 문제가 생겨도 다른 서버로 요청을 보낼 수 있음

---

## 6. HTTPS 처리

실제 서비스에서는 보안을 위해 `http://`보다 `https://`를 많이 사용합니다.

NGINX는 SSL 인증서를 설정해서 HTTPS 요청을 처리할 수 있습니다.

```text
https://example.com
```

즉, NGINX는 사용자의 브라우저와 서버 사이의 보안 연결을 담당할 수 있습니다.

---

## 7. 웹 서비스 구조에서 NGINX의 위치

일반적인 웹 서비스 구조는 다음과 같습니다.

```text
[사용자 브라우저]
        ↓ HTTP 요청
[NGINX]
        ↓ 필요하면 전달
[백엔드 서버]
        ↓
[DB]
```

예를 들어 로그인 요청의 흐름은 다음과 같습니다.

```text
브라우저: 로그인 요청
↓
NGINX: 이 요청은 백엔드 서버로 전달
↓
백엔드 서버: 아이디와 비밀번호 확인
↓
DB: 사용자 정보 조회
↓
백엔드 서버: 로그인 성공 응답
↓
NGINX
↓
브라우저
```

반대로 이미지나 CSS 파일 요청은 백엔드 서버까지 가지 않고 NGINX가 바로 처리할 수 있습니다.

```text
브라우저: logo.png 주세요
↓
NGINX: 여기 있습니다
```

---

## 8. 기본 NGINX 설정 예시

가장 기본적인 NGINX 설정은 다음과 같습니다.

```nginx
server {
    listen 80;
    server_name example.com;

    root /var/www/html;
    index index.html;

    location / {
        try_files $uri $uri/ =404;
    }
}
```

### 설정 설명

```nginx
listen 80;
```

80번 포트에서 HTTP 요청을 받겠다는 의미입니다.

```nginx
server_name example.com;
```

`example.com`으로 들어온 요청을 이 서버 블록이 처리한다는 의미입니다.

```nginx
root /var/www/html;
```

웹 파일들이 저장된 기본 폴더 위치입니다.

```nginx
index index.html;
```

사용자가 `/` 경로로 접속했을 때 기본으로 보여줄 파일입니다.

```nginx
location / {
    try_files $uri $uri/ =404;
}
```

요청한 파일이 있으면 보내고, 없으면 `404 Not Found`를 응답하라는 의미입니다.

---

## 9. 리버스 프록시 설정 예시

백엔드 서버가 `localhost:3000`에서 실행 중이라면 NGINX 설정은 다음과 같이 할 수 있습니다.

```nginx
server {
    listen 80;
    server_name example.com;

    location / {
        proxy_pass http://localhost:3000;
    }
}
```

이 설정의 의미는 다음과 같습니다.

```text
example.com으로 들어온 요청을 localhost:3000으로 넘겨라
```

React, Vue 같은 프론트엔드와 Node.js, Spring, FastAPI 같은 백엔드를 연결할 때 자주 사용하는 구조입니다.

---

## 10. NGINX와 백엔드 서버의 차이

NGINX와 백엔드 서버는 역할이 다릅니다.

| 구분 | 역할 |
|---|---|
| NGINX | 요청을 받고 정적 파일을 제공하거나 다른 서버로 전달 |
| 백엔드 서버 | 로그인, 회원가입, DB 처리, 비즈니스 로직 수행 |
| DB | 실제 데이터 저장 |

예를 들어 쇼핑몰이라면 다음과 같이 역할이 나뉩니다.

```text
NGINX:
상품 이미지, CSS 파일 제공
요청을 백엔드로 전달

백엔드:
회원가입 처리
주문 처리
결제 로직 처리

DB:
회원 정보 저장
상품 정보 저장
주문 정보 저장
```

---

## 11. 42 webserv 과제와의 연결

42의 `webserv` 과제는 쉽게 말하면 다음과 같습니다.

> “NGINX 같은 간단한 HTTP 웹 서버를 직접 C++로 구현해보기”

즉, `webserv` 과제에서 구현해야 하는 개념들은 NGINX가 실제로 하는 일과 많이 연결되어 있습니다.

| webserv 개념 | NGINX에서의 역할 |
|---|---|
| socket | 클라이언트 연결 받기 |
| bind / listen / accept | 포트 열고 요청 대기 |
| HTTP request parsing | GET, POST, DELETE 요청 해석 |
| HTTP response | 상태 코드와 body 응답 |
| config file | `nginx.conf` 같은 설정 파일 |
| location | URL 경로별 처리 |
| root | 정적 파일 경로 |
| index | 기본 파일 |
| error page | 404, 500 페이지 |
| CGI | PHP, Python 같은 외부 프로그램 실행 |
| body size limit | 업로드 크기 제한 |

따라서 NGINX 설정과 동작 방식을 이해하면 `webserv` 과제의 구조를 잡는 데 도움이 됩니다.

---

## 12. 쉬운 비유

NGINX는 웹사이트의 **프론트 데스크 직원**과 비슷합니다.

사용자가 말합니다.

```text
"메인 페이지 주세요"
```

그러면 NGINX가 바로 파일을 찾아서 줍니다.

```text
"여기 index.html 있습니다"
```

사용자가 또 말합니다.

```text
"로그인 처리해주세요"
```

그러면 NGINX가 직접 로그인 검사를 하는 것이 아니라, 뒤쪽 담당자인 백엔드 서버에게 요청을 넘깁니다.

```text
"이건 백엔드 서버가 처리해야겠네"
```

그리고 백엔드 서버의 응답을 다시 사용자에게 전달합니다.

---

## 13. 핵심 정리

NGINX는 **클라이언트의 HTTP 요청을 받아 처리하는 고성능 웹 서버**입니다.

핵심 역할은 다음과 같습니다.

```text
1. HTML, CSS, JS, 이미지 같은 정적 파일 제공
2. 백엔드 서버로 요청 전달
3. 여러 서버로 요청 분산
4. HTTPS 처리
5. 웹 서비스 앞단에서 트래픽 관리
```

42 `webserv` 과제 관점에서는 NGINX를 다음과 같이 이해하면 됩니다.

> “내가 직접 만들어야 하는 웹 서버의 실제 산업용 완성형 예시”
