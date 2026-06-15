# STRUCTURE.md

# 소켓 프로그래밍과 이벤트 기반 서버 구조 정리

이 문서는 이번 대화에서 설명한 내용을 바탕으로, **소켓 프로그래밍**, **ROS 토픽과의 비교**, 그리고 **42 webserv 과제 관점의 이벤트 기반 웹서버 구조**를 자세히 정리한 문서이다.

핵심 목표는 다음과 같다.

```text
1. 소켓이 무엇인지 이해한다.
2. 소켓과 ROS 토픽이 어떤 느낌으로 비슷하고 다른지 이해한다.
3. 웹서버가 socket → bind → listen → accept → read → parse → response → write → close 흐름으로 동작한다는 것을 이해한다.
4. 42 webserv에서 왜 non-blocking, poll, buffer, state machine이 중요한지 이해한다.
5. 이벤트 기반 서버가 여러 클라이언트를 어떻게 동시에 처리하는지 구조적으로 이해한다.
```

---

# 1. 소켓 프로그래밍이란?

**소켓 프로그래밍**은 프로그램끼리 네트워크를 통해 데이터를 주고받기 위해 사용하는 프로그래밍 방식이다.

쉽게 말하면, 소켓은 다음과 같이 이해할 수 있다.

```text
소켓 = 네트워크 통신을 위한 데이터 통로
```

리눅스나 유닉스 계열 운영체제에서는 소켓도 파일처럼 다뤄진다.  
즉, 소켓을 생성하면 정수형 파일 디스크립터, 즉 `fd`가 만들어진다.

```cpp
int server_fd = socket(AF_INET, SOCK_STREAM, 0);
```

여기서 `server_fd`는 파일 디스크립터이다.  
파일을 `read()`, `write()`로 읽고 쓰듯이, 소켓도 `recv()`, `send()` 또는 `read()`, `write()`를 통해 데이터를 주고받을 수 있다.

---

# 2. 소켓을 왜 사용하는가?

웹 브라우저가 웹서버에 접속하는 상황을 생각해보자.

```text
브라우저 ───── HTTP 요청 ─────> 웹서버
브라우저 <──── HTTP 응답 ───── 웹서버
```

예를 들어 사용자가 브라우저에서 다음 주소에 접속한다고 하자.

```text
http://localhost:8080
```

그러면 브라우저는 `localhost`라는 주소의 `8080번 포트`로 TCP 연결을 시도한다.

웹서버는 8080번 포트에서 기다리고 있다가 클라이언트 연결을 받아들이고, 요청을 읽은 뒤 응답을 보낸다.

이때 사용되는 핵심 기술이 바로 **소켓**이다.

---

# 3. TCP 서버의 기본 흐름

일반적인 TCP 서버는 다음 흐름으로 동작한다.

```text
socket → bind → listen → accept → read → write → close
```

각 단계의 의미는 다음과 같다.

```text
socket  : 소켓 생성
bind    : IP 주소와 포트 번호를 소켓에 연결
listen  : 클라이언트 접속 대기 상태로 전환
accept  : 클라이언트 연결 수락
read    : 클라이언트 요청 읽기
write   : 클라이언트에게 응답 보내기
close   : 연결 종료
```

C/C++ 코드 흐름으로 보면 대략 다음과 같다.

```cpp
int server_fd = socket(AF_INET, SOCK_STREAM, 0);

bind(server_fd, ...);

listen(server_fd, SOMAXCONN);

int client_fd = accept(server_fd, ...);

read(client_fd, buffer, sizeof(buffer));

write(client_fd, response, strlen(response));

close(client_fd);
close(server_fd);
```

---

# 4. 서버 소켓과 클라이언트 소켓

서버 프로그램에서는 보통 소켓이 두 종류로 나뉜다.

## 4.1 서버 소켓

서버 소켓은 클라이언트의 접속을 기다리는 소켓이다.

```cpp
int server_fd = socket(AF_INET, SOCK_STREAM, 0);
```

이 소켓은 특정 클라이언트와 직접 데이터를 주고받는 용도가 아니라, 클라이언트의 접속 요청을 받기 위한 용도이다.

비유하면 다음과 같다.

```text
server_fd = 가게 입구
```

---

## 4.2 클라이언트 소켓

클라이언트가 접속하면 서버는 `accept()`를 호출한다.

```cpp
int client_fd = accept(server_fd, NULL, NULL);
```

이때 생성되는 `client_fd`가 실제 클라이언트와 데이터를 주고받는 소켓이다.

비유하면 다음과 같다.

```text
client_fd = 접속한 손님과 대화하는 전용 자리
```

구조는 다음과 같다.

```text
server_fd
  ├── client_fd 1
  ├── client_fd 2
  ├── client_fd 3
  └── client_fd 4
```

즉, `server_fd`는 계속 새로운 접속을 기다리고, 각 클라이언트와의 실제 통신은 `client_fd`를 통해 이루어진다.

---

# 5. 소켓과 ROS 토픽은 비슷한가?

대화에서 나온 질문은 다음과 같았다.

```text
소켓이 ROS에서 토픽이랑 비슷한 느낌이야?
데이터가 흐르는 통로라고 이해하면 편할까?
```

결론부터 말하면 다음과 같다.

```text
소켓과 ROS 토픽은 둘 다 데이터가 흐르는 통로처럼 이해할 수 있다.
하지만 소켓은 더 낮은 수준의 통신 기술이고,
ROS 토픽은 더 높은 수준의 통신 개념이다.
```

---

# 6. 소켓과 ROS 토픽의 공통점

둘 다 데이터를 전달하는 역할을 한다.

ROS에서 `/cmd_vel` 토픽은 로봇에게 속도 명령을 보내는 통로처럼 느껴진다.

```text
teleop node ─── /cmd_vel ───> turtlebot node
```

소켓도 마찬가지로 클라이언트와 서버 사이에서 데이터를 주고받는 통로이다.

```text
브라우저 ─── TCP socket ─── 웹서버
```

따라서 처음 이해할 때는 다음처럼 생각해도 좋다.

```text
소켓도 토픽처럼 데이터가 흐르는 통로다.
```

---

# 7. 소켓과 ROS 토픽의 차이점

## 7.1 소켓은 낮은 수준의 직접 연결에 가깝다

소켓은 보통 다음과 같은 구조이다.

```text
A 프로그램 ───── 연결 ───── B 프로그램
```

예를 들어 웹 브라우저와 웹서버는 TCP 소켓으로 직접 연결된다.

```text
브라우저 ─── TCP socket ─── 웹서버
```

서버는 특정 포트를 열고 기다리고, 클라이언트는 그 포트로 접속한다.

---

## 7.2 ROS 토픽은 발행/구독 구조이다

ROS 토픽은 직접 상대방을 지정하지 않는다.

```text
Publisher ─── Topic ─── Subscriber
```

예를 들어 `/scan` 토픽에 라이다 노드가 데이터를 발행하면, 여러 노드가 동시에 받아볼 수 있다.

```text
LiDAR Node ─── /scan ───> Navigation Node
                     └──> RViz
                     └──> Debug Node
```

즉, ROS 토픽은 전화 통화보다는 라디오 방송 채널에 가깝다.

---

# 8. 소켓과 ROS 토픽의 비유

## 소켓

```text
소켓 = 전화 통화
```

A가 B에게 직접 연결해서 데이터를 주고받는 느낌이다.

---

## ROS 토픽

```text
ROS 토픽 = 라디오 방송 채널
```

누군가 특정 채널에 데이터를 발행하면, 그 채널을 구독 중인 여러 노드가 데이터를 받아볼 수 있다.

---

# 9. ROS2 토픽 내부 구조와 소켓

ROS2에서는 토픽 통신을 할 때 내부적으로 DDS라는 통신 미들웨어를 사용한다.

사용자는 코드에서 다음처럼 간단히 사용한다.

```cpp
publisher->publish(msg);
```

하지만 내부적으로는 대략 다음 계층을 거친다고 이해할 수 있다.

```text
ROS2 Topic
   ↓
DDS
   ↓
UDP/TCP 기반 네트워크 통신
   ↓
OS socket
   ↓
랜카드 / 네트워크
```

즉, ROS 토픽은 소켓보다 상위 개념이다.

정리하면 다음과 같다.

| 구분 | 소켓 | ROS 토픽 |
|---|---|---|
| 개념 수준 | 낮은 수준 | 높은 수준 |
| 구조 | 연결 중심 | 발행/구독 중심 |
| 사용 예 | 웹서버, 채팅 서버 | `/cmd_vel`, `/scan`, `/odom` |
| 상대방 지정 | 보통 직접 연결 | 상대방을 몰라도 됨 |
| 데이터 형식 | 직접 정해야 함 | 메시지 타입이 정해져 있음 |
| 비유 | 전화 통화 | 라디오 방송 |

---

# 10. 블로킹 서버의 문제

가장 단순한 서버는 다음처럼 작성할 수 있다.

```cpp
while (true)
{
    int client_fd = accept(server_fd, NULL, NULL);
    read(client_fd, buffer, sizeof(buffer));
    write(client_fd, response, strlen(response));
    close(client_fd);
}
```

이 구조는 이해하기 쉽지만 문제가 있다.

`accept()`와 `read()`는 기본적으로 blocking 함수이다.  
즉, 작업이 완료될 때까지 서버가 멈춘다.

예를 들어 어떤 클라이언트가 연결만 하고 데이터를 보내지 않는다고 하자.

```text
서버: read()에서 계속 대기
다른 클라이언트: 접속하고 싶지만 기다려야 함
```

이렇게 되면 한 클라이언트 때문에 서버 전체가 멈출 수 있다.

---

# 11. 멀티프로세스 / 멀티스레드 서버

블로킹 문제를 해결하는 쉬운 방법은 클라이언트마다 프로세스나 스레드를 만드는 것이다.

```text
클라이언트 1 → 스레드 1
클라이언트 2 → 스레드 2
클라이언트 3 → 스레드 3
```

장점은 구조가 직관적이라는 것이다.

하지만 단점도 크다.

```text
1. 클라이언트 수가 많아지면 스레드가 너무 많이 생긴다.
2. 메모리 사용량이 증가한다.
3. 문맥 교환 비용이 커진다.
4. 동기화 문제가 생길 수 있다.
5. 수천, 수만 개의 연결을 처리하기 어렵다.
```

그래서 고성능 서버에서는 이벤트 기반 구조를 많이 사용한다.

---

# 12. 이벤트 기반 서버란?

이벤트 기반 서버는 클라이언트마다 스레드를 하나씩 만들지 않는다.

대신 하나 또는 적은 수의 스레드가 여러 소켓을 동시에 관리한다.

핵심 아이디어는 다음과 같다.

```text
읽을 준비가 된 소켓만 읽고,
쓸 준비가 된 소켓만 쓴다.
```

즉, 서버는 모든 클라이언트를 붙잡고 기다리지 않는다.  
운영체제에게 다음과 같이 물어본다.

```text
지금 읽을 수 있는 소켓이 있는가?
지금 쓸 수 있는 소켓이 있는가?
새 접속이 들어온 소켓이 있는가?
연결이 끊긴 소켓이 있는가?
```

이 질문에 대한 답을 얻기 위해 `poll`, `select`, `epoll`, `kqueue` 같은 시스템 콜을 사용한다.

---

# 13. 이벤트 기반 서버의 전체 구조

이벤트 기반 서버는 보통 다음 흐름으로 동작한다.

```text
서버 시작
  ↓
socket 생성
  ↓
bind
  ↓
listen
  ↓
server_fd를 non-blocking으로 설정
  ↓
이벤트 감시 목록에 server_fd 등록
  ↓
event loop 실행
  ↓
이벤트가 발생한 fd만 처리
```

전체 구조는 다음과 같다.

```text
[Event Loop]
     |
     |-- server_fd에 이벤트 발생?
     |       └── accept()로 새 클라이언트 등록
     |
     |-- client_fd에 읽기 이벤트 발생?
     |       └── recv()로 요청 읽기
     |
     |-- client_fd에 쓰기 이벤트 발생?
     |       └── send()로 응답 보내기
     |
     |-- 연결 종료 또는 에러?
             └── close()
```

---

# 14. non-blocking 소켓

이벤트 기반 서버에서는 소켓을 보통 non-blocking 모드로 설정한다.

기본 blocking 소켓은 데이터가 없으면 기다린다.

```text
read() 호출
데이터 없음
계속 대기
서버 멈춤
```

반면 non-blocking 소켓은 데이터가 없으면 바로 반환한다.

```text
read() 호출
데이터 없음
EAGAIN 또는 EWOULDBLOCK 반환
서버는 다른 작업 처리
```

설정은 보통 다음처럼 한다.

```cpp
fcntl(fd, F_SETFL, O_NONBLOCK);
```

non-blocking 구조에서는 다음처럼 동작할 수 있다.

```text
읽을 데이터가 있으면 읽는다.
읽을 데이터가 없으면 기다리지 않고 다른 클라이언트를 처리한다.
```

이것이 이벤트 기반 서버의 핵심이다.

---

# 15. poll / select / epoll / kqueue

여러 fd를 감시하기 위해 사용하는 대표적인 시스템 콜은 다음과 같다.

## 15.1 select

가장 오래된 방식이다.

```cpp
select(max_fd + 1, &read_fds, &write_fds, NULL, NULL);
```

단점은 관리할 수 있는 fd 수에 제한이 있고, 매번 전체 fd 집합을 검사해야 한다는 것이다.

---

## 15.2 poll

`select`보다 개선된 방식이다.

```cpp
poll(fds, nfds, timeout);
```

fd 제한 문제는 줄었지만, 여전히 전체 fd 배열을 검사한다.

42 `webserv` 과제에서 많이 사용하는 방식이다.

---

## 15.3 epoll

리눅스에서 고성능 서버를 만들 때 많이 사용하는 방식이다.

```cpp
epoll_create();
epoll_ctl();
epoll_wait();
```

이벤트가 발생한 fd를 효율적으로 알려주기 때문에 고성능 서버에 적합하다.

---

## 15.4 kqueue

macOS, BSD 계열에서 사용하는 이벤트 감시 방식이다.

42 과제를 macOS 환경에서 수행하는 경우 `kqueue`와 연결해서 이해할 수 있다.

---

# 16. poll 기반 Event Loop 예시

`poll()`을 사용하는 서버는 대략 다음 구조를 가진다.

```cpp
while (true)
{
    int ready = poll(fds.data(), fds.size(), -1);

    for (size_t i = 0; i < fds.size(); i++)
    {
        if (fds[i].revents & POLLIN)
        {
            if (fds[i].fd == server_fd)
                acceptNewClient();
            else
                readFromClient(fds[i].fd);
        }

        if (fds[i].revents & POLLOUT)
        {
            writeToClient(fds[i].fd);
        }

        if (fds[i].revents & (POLLHUP | POLLERR))
        {
            closeClient(fds[i].fd);
        }
    }
}
```

이 구조의 의미는 다음과 같다.

```text
1. poll()로 이벤트가 생길 때까지 기다린다.
2. server_fd에 POLLIN이 발생하면 새 클라이언트를 accept()한다.
3. client_fd에 POLLIN이 발생하면 요청을 읽는다.
4. client_fd에 POLLOUT이 발생하면 응답을 보낸다.
5. 에러나 연결 종료 이벤트가 발생하면 close()한다.
```

---

# 17. 42 webserv 과제 관점의 핵심

42 `webserv` 과제에서는 단순히 소켓을 열고 응답을 보내는 것만으로는 부족하다.

핵심은 다음과 같다.

```text
1. socket, bind, listen, accept 이해
2. non-blocking fd 설정
3. poll / select / kqueue / epoll 중 하나로 이벤트 감시
4. 클라이언트별 request buffer 관리
5. HTTP 요청 파싱
6. HTTP 응답 생성
7. GET, POST, DELETE 처리
8. 파일 업로드 처리
9. CGI 처리
10. 에러 페이지 처리
11. keep-alive와 connection close 처리
12. 한 클라이언트 때문에 서버 전체가 멈추지 않게 만들기
```

`webserv`는 작은 NGINX 같은 서버를 직접 만드는 과제라고 이해하면 된다.

---

# 18. webserv의 실제 요청 처리 흐름

웹서버가 실제로 요청 하나를 처리하는 흐름은 다음과 같다.

```text
1. 브라우저가 localhost:8080에 접속한다.
2. server_fd에 POLLIN 이벤트가 발생한다.
3. 서버가 accept()를 호출한다.
4. client_fd가 생성된다.
5. client_fd를 non-blocking으로 설정한다.
6. client_fd를 poll 감시 목록에 등록한다.
7. 브라우저가 HTTP 요청을 보낸다.
8. client_fd에 POLLIN 이벤트가 발생한다.
9. 서버가 recv()로 요청 데이터를 읽는다.
10. request_buffer에 데이터를 누적한다.
11. HTTP 요청이 완성됐는지 확인한다.
12. 완성됐으면 HTTP 요청을 파싱한다.
13. 설정 파일 기준으로 server/location을 매칭한다.
14. GET, POST, DELETE, CGI 등 필요한 처리를 한다.
15. HTTP 응답을 만든다.
16. client_fd를 POLLOUT 감시 대상으로 바꾼다.
17. send()로 응답을 보낸다.
18. 응답을 다 보냈는지 확인한다.
19. keep-alive면 다음 요청을 기다린다.
20. close면 fd를 닫고 poll 목록에서 제거한다.
```

---

# 19. HTTP 요청 파싱

브라우저가 보내는 HTTP 요청은 단순한 문자열 데이터이다.

예를 들어 다음과 같은 요청이 들어올 수 있다.

```http
GET /index.html HTTP/1.1
Host: localhost:8080
User-Agent: Chrome
Connection: keep-alive

```

서버는 이 문자열을 분석해서 다음 정보를 뽑아야 한다.

```text
Method  : GET
URI     : /index.html
Version : HTTP/1.1
Headers : Host, User-Agent, Connection 등
Body    : POST 요청일 때 포함될 수 있음
```

파싱 결과는 다음과 같은 구조로 저장할 수 있다.

```cpp
class HttpRequest
{
public:
    std::string method;
    std::string uri;
    std::string version;
    std::map<std::string, std::string> headers;
    std::string body;
};
```

---

# 20. Request Line 파싱

HTTP 요청의 첫 줄은 Request Line이다.

```http
GET /index.html HTTP/1.1
```

이를 나누면 다음과 같다.

```text
GET         : Method
/index.html : URI
HTTP/1.1    : Version
```

잘못된 요청이면 적절한 에러를 반환해야 한다.

```text
잘못된 문법          → 400 Bad Request
지원하지 않는 메서드  → 405 Method Not Allowed
지원하지 않는 버전    → 505 HTTP Version Not Supported
```

---

# 21. Header 파싱

Request Line 다음에는 Header들이 온다.

```http
Host: localhost:8080
Content-Length: 123
Connection: keep-alive
```

헤더는 key-value 구조로 파싱할 수 있다.

```text
Host           → localhost:8080
Content-Length → 123
Connection     → keep-alive
```

중요한 헤더는 다음과 같다.

```text
Host
Content-Length
Transfer-Encoding
Connection
Content-Type
```

---

# 22. Body 파싱

`POST` 요청이나 파일 업로드 요청에서는 Body가 들어온다.

예를 들어 다음과 같다.

```http
POST /upload HTTP/1.1
Host: localhost:8080
Content-Length: 11

hello world
```

여기서 `Content-Length: 11`은 Body가 11바이트라는 뜻이다.

서버는 다음을 확인해야 한다.

```text
1. 헤더가 끝났는가?
2. Content-Length가 있는가?
3. body가 Content-Length만큼 다 들어왔는가?
```

아직 Body가 덜 들어왔으면 응답을 만들면 안 된다.  
더 읽어야 한다.

---

# 23. read()가 한 번에 끝난다고 가정하면 안 되는 이유

TCP는 스트림 기반 프로토콜이다.  
즉, 메시지 경계를 보장하지 않는다.

클라이언트가 하나의 HTTP 요청을 보냈다고 해서 서버의 `read()` 한 번에 요청 하나가 정확히 들어오는 것이 아니다.

---

## 23.1 요청이 쪼개져서 들어오는 경우

클라이언트가 보낸 요청은 다음과 같다고 하자.

```http
GET /index.html HTTP/1.1
Host: localhost

```

서버에서는 다음처럼 쪼개져서 읽힐 수 있다.

```text
첫 번째 read:
GET /inde

두 번째 read:
x.html HTTP/1.1\r\nHost:

세 번째 read:
 localhost\r\n\r\n
```

따라서 서버는 읽은 내용을 계속 `request_buffer`에 누적해야 한다.

---

## 23.2 요청 두 개가 한 번에 들어오는 경우

keep-alive 상황에서는 반대로 요청 두 개가 한 번에 들어올 수도 있다.

```text
첫 번째 read:
GET /index.html HTTP/1.1
Host: localhost

GET /style.css HTTP/1.1
Host: localhost

```

이 경우 첫 번째 요청만 처리하고, 두 번째 요청 데이터는 다음 요청으로 남겨둬야 한다.

---

# 24. 클라이언트별 request buffer 관리

이런 이유 때문에 클라이언트마다 별도의 버퍼가 필요하다.

```cpp
class Client
{
public:
    int fd;
    std::string request_buffer;
    std::string response_buffer;
    size_t bytes_sent;
    bool keep_alive;
    int state;
};
```

읽을 때마다 다음처럼 누적한다.

```cpp
char buffer[4096];
ssize_t n = recv(client_fd, buffer, sizeof(buffer), 0);

if (n > 0)
{
    clients[client_fd].request_buffer.append(buffer, n);
}
```

그 다음 다음 조건을 확인한다.

```text
1. HTTP 헤더가 끝났는가?
2. Content-Length가 있는가?
3. Body가 Content-Length만큼 들어왔는가?
4. 요청 크기가 너무 크지는 않은가?
5. chunked 요청이라면 chunk를 모두 받았는가?
```

---

# 25. HTTP 응답 생성

요청 파싱이 끝나면 서버는 응답을 만들어야 한다.

응답은 크게 세 부분으로 이루어진다.

```text
Status Line
Headers
Body
```

예시는 다음과 같다.

```http
HTTP/1.1 200 OK
Content-Type: text/html
Content-Length: 42
Connection: close

<html><body>Hello</body></html>
```

---

# 26. Status Code

상황에 따라 적절한 상태 코드를 반환해야 한다.

```text
200 OK                  : 요청 성공
201 Created             : 리소스 생성 성공
204 No Content          : 성공했지만 응답 Body 없음
301 Moved Permanently   : 리다이렉션
400 Bad Request         : 잘못된 요청
403 Forbidden           : 권한 없음
404 Not Found           : 파일 없음
405 Method Not Allowed  : 허용되지 않은 메서드
413 Payload Too Large   : 요청 Body가 너무 큼
500 Internal Server Error : 서버 내부 오류
505 HTTP Version Not Supported : HTTP 버전 미지원
```

---

# 27. GET 처리

`GET`은 파일이나 리소스를 가져오는 요청이다.

```http
GET /index.html HTTP/1.1
```

서버는 다음을 처리해야 한다.

```text
1. URI 확인
2. 설정 파일의 location 매칭
3. root와 URI를 합쳐 실제 파일 경로 생성
4. 파일 존재 여부 확인
5. 읽기 권한 확인
6. 디렉토리라면 index 또는 autoindex 처리
7. 파일을 읽어 HTTP 응답 생성
```

예를 들어 설정이 다음과 같다고 하자.

```nginx
root /var/www/html;
```

요청이 다음과 같다면,

```http
GET /index.html HTTP/1.1
```

실제 파일 경로는 다음처럼 해석될 수 있다.

```text
/var/www/html/index.html
```

---

# 28. POST 처리

`POST`는 서버에 데이터를 보내는 요청이다.

예를 들어 파일 업로드나 폼 제출에 사용된다.

```http
POST /upload HTTP/1.1
Content-Length: 1234
Content-Type: multipart/form-data

...
```

서버는 다음을 처리해야 한다.

```text
1. Body 전체 수신
2. Content-Length 또는 Transfer-Encoding 확인
3. client_max_body_size 초과 여부 확인
4. 업로드 경로 확인
5. 파일 저장 또는 CGI 프로그램으로 전달
6. 성공 또는 실패 응답 생성
```

---

# 29. DELETE 처리

`DELETE`는 서버의 리소스를 삭제하는 요청이다.

```http
DELETE /file.txt HTTP/1.1
```

서버는 다음을 확인해야 한다.

```text
1. 해당 location에서 DELETE가 허용되는가?
2. 실제 파일이 존재하는가?
3. 삭제 권한이 있는가?
4. 디렉토리 삭제 요청은 어떻게 처리할 것인가?
5. 삭제 성공 또는 실패 응답을 어떻게 만들 것인가?
```

성공하면 보통 다음 상태를 반환할 수 있다.

```text
200 OK
204 No Content
```

---

# 30. 파일 업로드 처리

파일 업로드는 보통 `POST` 요청으로 처리된다.

브라우저가 파일을 업로드할 때는 `multipart/form-data` 형식을 사용하는 경우가 많다.

```http
POST /upload HTTP/1.1
Content-Type: multipart/form-data; boundary=----abc
Content-Length: 10000

------abc
Content-Disposition: form-data; name="file"; filename="test.txt"
Content-Type: text/plain

hello world
------abc--
```

서버는 다음을 해야 한다.

```text
1. boundary 찾기
2. 각 part 분리
3. 파일 이름 파싱
4. 파일 내용 추출
5. 업로드 디렉토리에 저장
6. 성공 응답 반환
```

중요한 점은 업로드 Body도 한 번에 다 들어온다고 보장할 수 없다는 것이다.

따라서 다음처럼 처리해야 한다.

```text
조금 받기
버퍼에 저장하기
아직 덜 받았으면 기다리기
다 받으면 파일로 저장하기
```

---

# 31. CGI 처리

CGI는 서버가 직접 응답을 만드는 것이 아니라, 외부 프로그램을 실행해서 그 결과를 응답으로 보내는 방식이다.

예를 들어 다음 요청이 들어왔다고 하자.

```http
GET /cgi-bin/test.py HTTP/1.1
```

서버는 Python 스크립트를 실행하고, 그 출력 결과를 HTTP 응답으로 클라이언트에게 보낸다.

구조는 다음과 같다.

```text
클라이언트
   ↓
webserv
   ↓
CGI 프로그램 실행
   ↓
CGI stdout 결과
   ↓
webserv가 HTTP 응답으로 전송
```

구현에는 다음 시스템 콜이 필요할 수 있다.

```text
pipe
fork
execve
waitpid
dup2
```

CGI에서 중요한 환경변수 예시는 다음과 같다.

```text
REQUEST_METHOD=POST
SCRIPT_NAME=/test.py
CONTENT_LENGTH=123
CONTENT_TYPE=application/x-www-form-urlencoded
QUERY_STRING=name=kyu
```

POST 요청의 Body는 CGI 프로그램의 stdin으로 넘기고,  
CGI 프로그램의 stdout을 읽어서 클라이언트에게 응답으로 보내야 한다.

---

# 32. 에러 페이지 처리

웹서버는 정상 요청뿐만 아니라 잘못된 요청도 처리해야 한다.

예를 들어 다음 상황들이 있다.

```text
파일이 없음       → 404 Not Found
권한이 없음       → 403 Forbidden
요청이 너무 큼    → 413 Payload Too Large
서버 내부 오류    → 500 Internal Server Error
잘못된 요청 문법  → 400 Bad Request
```

설정 파일에서 에러 페이지를 지정할 수도 있다.

```nginx
error_page 404 /404.html;
```

이 경우 404가 발생했을 때 기본 에러 메시지를 보내는 대신 `/404.html`을 읽어서 응답 Body로 보낼 수 있다.

---

# 33. keep-alive와 Connection: close

HTTP/1.1에서는 기본적으로 연결을 바로 닫지 않고 유지할 수 있다.  
이것을 keep-alive라고 한다.

브라우저가 웹페이지 하나를 열 때 실제로는 여러 리소스를 요청한다.

```text
index.html
style.css
main.js
logo.png
```

매번 TCP 연결을 새로 만들면 비효율적이므로, 하나의 연결에서 여러 요청을 처리할 수 있다.

```text
client_fd 하나로
  GET /index.html
  GET /style.css
  GET /main.js
```

---

## 33.1 Connection: close

클라이언트가 다음 헤더를 보낸 경우,

```http
Connection: close
```

서버는 응답을 보낸 뒤 연결을 닫아야 한다.

```text
응답 전송 완료
↓
close(client_fd)
```

---

## 33.2 keep-alive 처리 흐름

keep-alive에서는 한 요청을 처리한 뒤 fd를 바로 닫지 않는다.

```text
요청 1 읽기
응답 1 보내기
버퍼 정리
다시 요청 2 읽기
응답 2 보내기
...
```

따라서 클라이언트 상태 관리가 중요하다.

---

# 34. write()도 한 번에 끝난다고 가정하면 안 된다

`read()`만 문제가 되는 것이 아니다.  
`write()` 또는 `send()`도 한 번에 모든 데이터를 보내지 못할 수 있다.

예를 들어 1MB 응답을 보내려고 했는데 실제로는 일부만 전송될 수 있다.

```text
보내려고 한 크기: 1,000,000 bytes
실제로 보낸 크기: 32,768 bytes
```

그러면 나머지를 다음 `POLLOUT` 이벤트 때 이어서 보내야 한다.

이를 위해 클라이언트마다 다음 정보가 필요하다.

```cpp
std::string response_buffer;
size_t bytes_sent;
```

전송 흐름은 다음과 같다.

```text
response_buffer 생성
↓
POLLOUT 이벤트 발생
↓
send() 호출
↓
일부 전송
↓
bytes_sent 업데이트
↓
아직 남았으면 다음 POLLOUT 기다림
↓
다 보냈으면 close 또는 keep-alive 상태로 전환
```

---

# 35. 클라이언트 상태 머신

이벤트 기반 서버에서는 클라이언트마다 현재 상태를 저장해야 한다.

예를 들어 다음과 같은 상태를 둘 수 있다.

```cpp
enum ClientState
{
    READING_REQUEST,
    PARSING_REQUEST,
    BUILDING_RESPONSE,
    WRITING_RESPONSE,
    DONE,
    ERROR
};
```

상태 흐름은 다음과 같다.

```text
READING_REQUEST
   ↓ 요청이 완성됨
PARSING_REQUEST
   ↓ 파싱 성공
BUILDING_RESPONSE
   ↓ 응답 생성 완료
WRITING_RESPONSE
   ↓ 응답 전송 완료
DONE
   ↓
keep-alive면 READING_REQUEST로 복귀
close면 연결 종료
```

그림으로 보면 다음과 같다.

```text
[READ]
   ↓
[PARSE]
   ↓
[BUILD RESPONSE]
   ↓
[WRITE]
   ↓
[KEEP-ALIVE?]
   ├── yes → [READ]
   └── no  → [CLOSE]
```

---

# 36. 한 클라이언트 때문에 서버 전체가 멈추면 안 된다

이벤트 기반 서버의 가장 중요한 설계 원칙은 다음이다.

```text
어떤 클라이언트도 서버 전체를 멈추게 하면 안 된다.
```

예를 들어 나쁜 클라이언트가 연결만 하고 데이터를 아주 천천히 보낼 수 있다.

```text
1초에 한 글자씩 보냄
```

만약 서버가 그 클라이언트의 요청이 완성될 때까지 기다리면 다른 클라이언트가 모두 피해를 본다.

따라서 서버는 다음처럼 동작해야 한다.

```text
읽을 수 있는 만큼만 읽는다.
아직 요청이 완성되지 않았으면 상태를 저장한다.
다른 클라이언트를 처리한다.
나중에 이벤트가 다시 오면 이어서 읽는다.
```

이것이 non-blocking + event loop 구조의 본질이다.

---

# 37. 설정 파일과 server/location 매칭

`webserv`에서는 NGINX처럼 설정 파일을 읽어서 서버 동작을 결정해야 한다.

예시는 다음과 같다.

```nginx
server {
    listen 8080;
    server_name localhost;

    root /var/www/html;
    index index.html;

    client_max_body_size 1000000;

    error_page 404 /404.html;

    location /upload {
        methods GET POST;
        upload_path /tmp/uploads;
    }

    location /cgi-bin {
        cgi .py /usr/bin/python3;
    }
}
```

요청이 들어오면 서버는 다음을 판단해야 한다.

```text
1. 어떤 port로 들어왔는가?
2. Host 헤더가 무엇인가?
3. 어떤 server block과 매칭되는가?
4. URI가 어떤 location과 매칭되는가?
5. 해당 location에서 허용된 method인가?
6. root, index, autoindex, upload, cgi 설정은 무엇인가?
```

예를 들어 다음 요청이 들어왔다고 하자.

```http
GET /upload/test.txt HTTP/1.1
Host: localhost:8080
```

이 요청은 `/upload` location과 매칭될 수 있다.

---

# 38. webserv 구현 구조 예시

실제 구현에서는 역할별로 클래스를 나누는 것이 좋다.

예시는 다음과 같다.

```text
Server
  - server_fd
  - port
  - config

Client
  - client_fd
  - request_buffer
  - response_buffer
  - state
  - bytes_sent
  - keep_alive

HttpRequest
  - method
  - uri
  - version
  - headers
  - body

HttpResponse
  - status_code
  - headers
  - body

Config
  - servers
  - locations
  - root
  - index
  - error_page
  - max_body_size

EventLoop
  - poll fds
  - accept client
  - read client
  - write client
  - close client
```

---

# 39. 핵심 객체들의 역할

## 39.1 Server

서버 소켓과 서버 설정을 관리한다.

```text
포트 열기
server_fd 관리
listen 상태 유지
새 클라이언트 accept
```

---

## 39.2 Client

각 클라이언트 연결의 상태를 관리한다.

```text
client_fd
request_buffer
response_buffer
현재 상태
보낸 바이트 수
keep-alive 여부
```

---

## 39.3 HttpRequest

클라이언트가 보낸 HTTP 요청을 파싱한 결과를 저장한다.

```text
method
uri
version
headers
body
```

---

## 39.4 HttpResponse

클라이언트에게 보낼 HTTP 응답을 만든다.

```text
status_code
headers
body
```

---

## 39.5 Config

설정 파일의 내용을 저장한다.

```text
listen
server_name
root
index
error_page
client_max_body_size
location
allowed methods
cgi
upload_path
```

---

## 39.6 EventLoop

모든 fd를 감시하고, 이벤트가 발생한 fd만 처리한다.

```text
poll 실행
server_fd 이벤트 처리
client_fd 읽기 처리
client_fd 쓰기 처리
close 처리
```

---

# 40. webserv에서 실수하기 쉬운 부분

중요한 실수 포인트는 다음과 같다.

```text
1. read() 한 번에 요청이 다 온다고 가정하는 것
2. write() 한 번에 응답이 다 나간다고 가정하는 것
3. client마다 buffer와 state를 두지 않는 것
4. fd를 close한 뒤 poll 목록에서 제거하지 않는 것
5. non-blocking 설정을 하지 않는 것
6. keep-alive 요청을 무조건 close하는 것
7. Content-Length만큼 body를 다 받기 전에 POST 처리를 하는 것
8. 요청 크기 제한을 확인하지 않는 것
9. 파일 권한, 존재 여부, 디렉토리 여부를 제대로 확인하지 않는 것
10. CGI 처리에서 pipe, fork, execve, timeout 관리를 빠뜨리는 것
11. 에러가 발생했을 때 적절한 status code를 반환하지 않는 것
12. server/location 설정 매칭을 잘못하는 것
```

---

# 41. 전체 구조 한눈에 보기

최종적인 서버 구조는 다음과 같이 이해할 수 있다.

```text
[브라우저]
    |
    | HTTP Request
    v
[server_fd]
    |
    | accept()
    v
[client_fd]
    |
    | recv()
    v
[request_buffer]
    |
    | HTTP 요청 완성 여부 확인
    v
[HttpRequest Parser]
    |
    | method / uri / header / body 분석
    v
[Config Matcher]
    |
    | server / location / method / root / cgi 확인
    v
[Handler]
    |
    | GET / POST / DELETE / CGI / upload 처리
    v
[HttpResponse Builder]
    |
    | status line / headers / body 생성
    v
[response_buffer]
    |
    | send()
    v
[브라우저]
```

이 전체 흐름을 이벤트 기반으로 돌리면 다음과 같다.

```text
Event Loop
   |
   |-- server_fd + POLLIN  → accept new client
   |
   |-- client_fd + POLLIN  → read request
   |
   |-- client_fd + POLLOUT → write response
   |
   |-- client_fd + ERROR   → close client
```

---

# 42. 최종 요약

소켓 프로그래밍은 서버와 클라이언트가 네트워크로 데이터를 주고받기 위한 기본 기술이다.

```text
소켓 = 네트워크 데이터 통로
```

ROS 토픽도 데이터가 흐르는 통로처럼 이해할 수 있지만, 소켓보다 더 높은 수준의 개념이다.

```text
소켓 = 실제 네트워크 연결 통로
ROS 토픽 = 노드들이 데이터를 쉽게 주고받도록 만든 논리적 채널
```

웹서버의 기본 흐름은 다음과 같다.

```text
socket → bind → listen → accept → read → parse → response → write → close
```

하지만 42 `webserv`에서는 여러 클라이언트를 동시에 처리해야 하므로 다음 개념들이 필요하다.

```text
non-blocking fd
poll / select / epoll / kqueue
client별 request buffer
client별 response buffer
HTTP parser
HTTP response builder
state machine
keep-alive 처리
CGI 처리
에러 페이지 처리
설정 파일 기반 server/location 매칭
```

가장 중요한 문장은 다음이다.

```text
서버는 클라이언트 하나를 끝까지 붙잡고 처리하는 것이 아니라,
각 클라이언트의 상태를 저장해두고,
이벤트가 올 때마다 조금씩 이어서 처리한다.
```

이것이 이벤트 기반 서버 구조의 핵심이며, 42 `webserv` 과제에서 반드시 이해해야 하는 부분이다.
