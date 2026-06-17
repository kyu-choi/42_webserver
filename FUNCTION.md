# FUNCTION.md

# 42 Webserv 허용 함수 정리

이 문서는 42 `webserv` 과제에서 허용된 함수들을 목적별로 정리한 문서이다.

과제 조건은 다음과 같다.

```text
All functionality must be implemented in C++ 98.
```

즉, 모든 기능은 **C++98** 기준으로 구현해야 하며, 허용된 시스템 함수만 사용해야 한다.

---

# 1. 전체 함수 목록

```text
execve, pipe, strerror, gai_strerror, errno, dup,
dup2, fork, socketpair, htons, htonl, ntohs, ntohl,
select, poll, epoll (epoll_create, epoll_ctl,
epoll_wait), kqueue (kqueue, kevent), socket,
accept, listen, send, recv, chdir, bind, connect,
getaddrinfo, freeaddrinfo, setsockopt, getsockname,
getprotobyname, fcntl, close, read, write, waitpid,
kill, signal, access, stat, open, opendir, readdir
and closedir.
```

---

# 2. 프로세스 / CGI 관련 함수

## execve

```c
int execve(const char *path, char *const argv[], char *const envp[]);
```

현재 프로세스를 다른 프로그램으로 교체해서 실행하는 함수이다.

webserv에서는 주로 **CGI 실행**에 사용한다.

예시:

```c
execve("/usr/bin/python3", argv, envp);
```

중요한 점은 `execve`가 성공하면 기존 코드로 돌아오지 않는다는 것이다.

---

## fork

```c
pid_t fork(void);
```

현재 프로세스를 복제해서 자식 프로세스를 만든다.

webserv에서는 CGI 실행 시 보통 `fork()` 후 자식 프로세스에서 `execve()`를 호출한다.

예시:

```c
pid_t pid = fork();

if (pid == 0)
{
    // child process
    execve(...);
}
else
{
    // parent process
}
```

---

## waitpid

```c
pid_t waitpid(pid_t pid, int *status, int options);
```

자식 프로세스가 종료되었는지 확인하거나 기다리는 함수이다.

CGI 프로세스가 종료되었는지 확인할 때 사용한다.

```c
waitpid(pid, &status, 0);
```

---

## kill

```c
int kill(pid_t pid, int sig);
```

특정 프로세스에 시그널을 보낸다.

CGI가 너무 오래 실행될 경우 종료시키는 데 사용할 수 있다.

```c
kill(pid, SIGKILL);
```

---

## signal

```c
void (*signal(int signum, void (*handler)(int)))(int);
```

시그널 처리 방식을 설정하는 함수이다.

예를 들어 `SIGPIPE`로 인해 서버가 종료되는 것을 방지할 수 있다.

```c
signal(SIGPIPE, SIG_IGN);
```

---

# 3. 파이프 / 파일 디스크립터 관련 함수

## pipe

```c
int pipe(int pipefd[2]);
```

프로세스 간 통신용 파이프를 만든다.

```c
int fd[2];
pipe(fd);
```

- `fd[0]`: 읽기용
- `fd[1]`: 쓰기용

webserv에서는 CGI와 서버 사이에 데이터를 주고받을 때 사용한다.

```text
server -> CGI stdin
CGI stdout -> server
```

---

## dup

```c
int dup(int oldfd);
```

파일 디스크립터를 복사한다.

```c
int newfd = dup(oldfd);
```

---

## dup2

```c
int dup2(int oldfd, int newfd);
```

파일 디스크립터를 원하는 번호로 복사한다.

CGI에서 표준 입력과 표준 출력을 파이프로 연결할 때 자주 사용한다.

```c
dup2(pipe_in[0], STDIN_FILENO);
dup2(pipe_out[1], STDOUT_FILENO);
```

---

## close

```c
int close(int fd);
```

파일 디스크립터를 닫는다.

소켓, 파일, 파이프 모두 닫을 수 있다.

```c
close(client_fd);
```

사용하지 않는 fd를 닫지 않으면 fd leak이 발생할 수 있다.

---

# 4. 에러 처리 관련 함수

## errno

```c
extern int errno;
```

시스템 콜이 실패했을 때 실패 원인을 담는 전역 변수이다.

```c
if (socket(...) == -1)
{
    // errno에 실패 이유가 저장됨
}
```

주의할 점:

```text
read/write 이후 errno 값을 보고 서버 동작을 조정하면 안 된다.
```

webserv 과제에서는 소켓 I/O 가능 여부를 `errno`가 아니라 `poll()` 또는 그에 준하는 이벤트 감시 함수로 판단해야 한다.

---

## strerror

```c
char *strerror(int errnum);
```

`errno` 값을 사람이 읽을 수 있는 문자열로 변환한다.

```c
std::cerr << strerror(errno) << std::endl;
```

예시 출력:

```text
Permission denied
Connection refused
```

---

## gai_strerror

```c
const char *gai_strerror(int errcode);
```

`getaddrinfo()` 실패 코드를 문자열로 변환한다.

```c
std::cerr << gai_strerror(ret) << std::endl;
```

---

# 5. 바이트 순서 변환 함수

네트워크에서는 보통 Big Endian 방식을 사용한다.
하지만 컴퓨터 내부에서는 Little Endian을 사용할 수도 있다.

따라서 포트 번호나 IP 주소를 네트워크 바이트 순서로 변환해야 한다.

---

## htons

```c
uint16_t htons(uint16_t hostshort);
```

Host To Network Short.

주로 포트 번호 변환에 사용한다.

```c
addr.sin_port = htons(8080);
```

---

## htonl

```c
uint32_t htonl(uint32_t hostlong);
```

Host To Network Long.

주로 32비트 IP 주소 값을 네트워크 바이트 순서로 변환할 때 사용한다.

```c
addr.sin_addr.s_addr = htonl(INADDR_ANY);
```

---

## ntohs

```c
uint16_t ntohs(uint16_t netshort);
```

Network To Host Short.

네트워크 형식의 16비트 값을 호스트 형식으로 변환한다.

---

## ntohl

```c
uint32_t ntohl(uint32_t netlong);
```

Network To Host Long.

네트워크 형식의 32비트 값을 호스트 형식으로 변환한다.

---

# 6. I/O 이벤트 감시 함수

webserv에서 가장 중요한 부분이다.

서버는 여러 클라이언트를 동시에 처리해야 한다.
하지만 `read()`나 `write()`에서 서버가 멈추면 안 된다.

그래서 `select`, `poll`, `epoll`, `kqueue` 같은 이벤트 감시 함수를 사용한다.

---

## select

```c
int select(int nfds, fd_set *readfds, fd_set *writefds,
           fd_set *exceptfds, struct timeval *timeout);
```

여러 fd 중에서 읽기 또는 쓰기 가능한 fd를 감시한다.

단점:

- fd 개수 제한이 있다.
- 매번 fd_set을 다시 설정해야 한다.
- 많은 클라이언트를 처리하기에는 불리하다.

---

## poll

```c
int poll(struct pollfd *fds, nfds_t nfds, int timeout);
```

여러 fd의 이벤트를 감시한다.

webserv에서 가장 구현하기 쉬운 방식 중 하나이다.

```c
struct pollfd pfd;
pfd.fd = server_fd;
pfd.events = POLLIN;
```

주요 이벤트:


| 이벤트       | 의미    |
| --------- | ----- |
| `POLLIN`  | 읽기 가능 |
| `POLLOUT` | 쓰기 가능 |
| `POLLERR` | 에러 발생 |
| `POLLHUP` | 연결 끊김 |


webserv 핵심 규칙:

```text
모든 socket I/O는 poll을 거쳐야 한다.
read/write를 바로 호출하면 안 된다.
```

---

## epoll_create

```c
int epoll_create(int size);
```

Linux 전용 이벤트 감시 방식인 epoll 인스턴스를 만든다.

---

## epoll_ctl

```c
int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);
```

epoll이 감시할 fd를 추가, 수정, 삭제한다.

주요 옵션:

- `EPOLL_CTL_ADD`
- `EPOLL_CTL_MOD`
- `EPOLL_CTL_DEL`

---

## epoll_wait

```c
int epoll_wait(int epfd, struct epoll_event *events,
               int maxevents, int timeout);
```

등록된 fd 중 이벤트가 발생할 때까지 기다린다.

`poll()`보다 많은 연결을 처리하는 데 유리하다.

---

## kqueue

```c
int kqueue(void);
```

BSD/macOS 계열에서 사용하는 이벤트 큐를 생성한다.

---

## kevent

```c
int kevent(int kq, const struct kevent *changelist, int nchanges,
           struct kevent *eventlist, int nevents,
           const struct timespec *timeout);
```

kqueue에 이벤트를 등록하거나 발생한 이벤트를 가져온다.

macOS에서는 `epoll` 대신 `kqueue`를 사용한다.

---

# 7. 소켓 생성 / 서버 연결 관련 함수

## socket

```c
int socket(int domain, int type, int protocol);
```

소켓을 생성한다.

TCP 서버에서는 보통 다음과 같이 사용한다.

```c
int server_fd = socket(AF_INET, SOCK_STREAM, 0);
```

주요 인자:


| 값             | 의미   |
| ------------- | ---- |
| `AF_INET`     | IPv4 |
| `AF_INET6`    | IPv6 |
| `SOCK_STREAM` | TCP  |
| `SOCK_DGRAM`  | UDP  |


---

## bind

```c
int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
```

소켓에 IP 주소와 포트 번호를 연결한다.

```c
bind(server_fd, (struct sockaddr *)&addr, sizeof(addr));
```

예를 들어 서버가 `0.0.0.0:8080`에서 요청을 받을 수 있게 만든다.

---

## listen

```c
int listen(int sockfd, int backlog);
```

소켓을 서버 대기 상태로 만든다.

```c
listen(server_fd, SOMAXCONN);
```

이후 클라이언트 연결 요청을 받을 수 있다.

---

## accept

```c
int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
```

클라이언트 연결을 수락한다.

```c
int client_fd = accept(server_fd, NULL, NULL);
```

- `server_fd`: 연결 요청을 받는 서버 소켓
- `client_fd`: 실제 클라이언트와 통신하는 소켓

---

## connect

```c
int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
```

클라이언트가 서버에 연결할 때 사용하는 함수이다.

webserv 서버 구현에서는 자주 사용하지 않는다.
하지만 서버가 외부 서버에 연결해야 하는 경우 사용할 수 있다.

---

## socketpair

```c
int socketpair(int domain, int type, int protocol, int sv[2]);
```

서로 연결된 소켓 두 개를 만든다.

```c
int sv[2];
socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
```

파이프와 비슷하게 프로세스 간 통신에 사용할 수 있다.

---

# 8. 데이터 송수신 함수

## send

```c
ssize_t send(int sockfd, const void *buf, size_t len, int flags);
```

소켓으로 데이터를 보낸다.

```c
send(client_fd, response.c_str(), response.size(), 0);
```

주의:

```text
send가 항상 전체 데이터를 한 번에 보내는 것은 아니다.
```

예를 들어 10KB를 보내려고 했는데 3KB만 전송될 수 있다.
따라서 남은 데이터를 버퍼에 저장하고 다음 `POLLOUT` 때 이어서 보내야 한다.

---

## recv

```c
ssize_t recv(int sockfd, void *buf, size_t len, int flags);
```

소켓에서 데이터를 읽는다.

```c
char buffer[4096];
ssize_t n = recv(client_fd, buffer, sizeof(buffer), 0);
```

주의:

```text
HTTP 요청이 한 번에 다 들어온다고 가정하면 안 된다.
```

요청이 여러 조각으로 나뉘어 들어올 수 있으므로 클라이언트별 request buffer가 필요하다.

---

## read

```c
ssize_t read(int fd, void *buf, size_t count);
```

파일 디스크립터에서 데이터를 읽는다.

소켓, 파일, 파이프 모두 읽을 수 있다.

```c
read(fd, buffer, 4096);
```

webserv에서는 다음 상황에서 사용된다.

- 일반 파일 읽기
- CGI pipe 읽기
- socket 읽기

단, socket이나 pipe처럼 blocking될 수 있는 fd는 반드시 non-blocking + poll 기반으로 처리해야 한다.

---

## write

```c
ssize_t write(int fd, const void *buf, size_t count);
```

파일 디스크립터에 데이터를 쓴다.

```c
write(fd, data, size);
```

webserv에서는 다음 상황에서 사용된다.

- 파일 저장
- CGI pipe 쓰기
- socket 응답 전송

---

# 9. 주소 정보 관련 함수

## getaddrinfo

```c
int getaddrinfo(const char *node, const char *service,
                const struct addrinfo *hints,
                struct addrinfo **res);
```

호스트 이름과 포트 정보를 소켓 주소 구조체로 변환한다.

```c
getaddrinfo("localhost", "8080", &hints, &res);
```

IPv4와 IPv6를 더 일반적으로 처리할 수 있다.

---

## freeaddrinfo

```c
void freeaddrinfo(struct addrinfo *res);
```

`getaddrinfo()`가 할당한 메모리를 해제한다.

```c
freeaddrinfo(res);
```

---

## getprotobyname

```c
struct protoent *getprotobyname(const char *name);
```

프로토콜 이름으로 프로토콜 정보를 가져온다.

```c
getprotobyname("tcp");
```

webserv에서는 자주 직접 사용하지 않는다.

---

# 10. 소켓 옵션 / 소켓 정보 함수

## setsockopt

```c
int setsockopt(int sockfd, int level, int optname,
               const void *optval, socklen_t optlen);
```

소켓 옵션을 설정한다.

webserv에서 자주 사용하는 옵션은 `SO_REUSEADDR`이다.

```c
int opt = 1;
setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
```

`SO_REUSEADDR`는 서버 재시작 시 포트를 바로 재사용할 수 있게 도와준다.

---

## getsockname

```c
int getsockname(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
```

소켓에 바인딩된 주소 정보를 가져온다.

현재 소켓이 어떤 IP와 포트에 연결되어 있는지 확인할 수 있다.

---

# 11. fd 설정 함수

## fcntl

```c
int fcntl(int fd, int cmd, ...);
```

파일 디스크립터의 속성을 변경한다.

webserv에서 가장 중요한 사용법은 **non-blocking 설정**이다.

```c
int flags = fcntl(fd, F_GETFL, 0);
fcntl(fd, F_SETFL, flags | O_NONBLOCK);
```

다음 fd들은 보통 non-blocking으로 설정해야 한다.

- server socket
- client socket
- CGI pipe

---

# 12. 디렉터리 / 경로 관련 함수

## chdir

```c
int chdir(const char *path);
```

현재 작업 디렉터리를 변경한다.

```c
chdir("/var/www/html");
```

CGI 실행 전에 특정 디렉터리로 이동해야 할 때 사용할 수 있다.

---

## opendir

```c
DIR *opendir(const char *name);
```

디렉터리를 연다.

```c
DIR *dir = opendir("./www");
```

webserv에서는 autoindex 기능을 구현할 때 사용할 수 있다.

---

## readdir

```c
struct dirent *readdir(DIR *dirp);
```

디렉터리 안의 파일 목록을 하나씩 읽는다.

```c
struct dirent *entry;
while ((entry = readdir(dir)) != NULL)
{
    std::cout << entry->d_name << std::endl;
}
```

---

## closedir

```c
int closedir(DIR *dirp);
```

열었던 디렉터리를 닫는다.

```c
closedir(dir);
```

---

# 13. 파일 접근 / 상태 확인 함수

## access

```c
int access(const char *pathname, int mode);
```

파일 접근 가능 여부를 확인한다.

```c
access(path, R_OK);
access(path, W_OK);
access(path, X_OK);
access(path, F_OK);
```


| 옵션     | 의미       |
| ------ | -------- |
| `F_OK` | 파일 존재 여부 |
| `R_OK` | 읽기 가능 여부 |
| `W_OK` | 쓰기 가능 여부 |
| `X_OK` | 실행 가능 여부 |


예시:

```c
if (access("./index.html", R_OK) == 0)
{
    // 읽을 수 있음
}
```

---

## stat

```c
int stat(const char *pathname, struct stat *statbuf);
```

파일이나 디렉터리 정보를 가져온다.

```c
struct stat st;
stat(path.c_str(), &st);
```

확인 가능한 정보:

- 파일인지 여부
- 디렉터리인지 여부
- 파일 크기
- 권한
- 수정 시간

예시:

```c
if (S_ISDIR(st.st_mode))
{
    // 디렉터리
}
else if (S_ISREG(st.st_mode))
{
    // 일반 파일
}
```

---

## open

```c
int open(const char *pathname, int flags, mode_t mode);
```

파일을 연다.

정적 파일 응답 예시:

```c
int fd = open("./index.html", O_RDONLY);
```

POST 업로드 파일 생성 예시:

```c
int fd = open("./upload/file.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
```

주의:

이 함수 목록에는 `remove()`나 `unlink()`가 없다.
따라서 DELETE 메서드 구현 방식은 subject PDF에서 실제 허용 함수 목록을 다시 확인해야 한다.

---

# 14. webserv 실행 흐름에서의 함수 사용

## 14.1 서버 시작

```text
socket
setsockopt
fcntl
bind
listen
poll
```

서버 소켓을 만들고, 포트를 바인딩한 뒤, non-blocking으로 설정하고, `poll()`로 이벤트를 감시한다.

---

## 14.2 클라이언트 연결

```text
poll에서 server_fd가 POLLIN
accept
fcntl(client_fd, O_NONBLOCK)
poll 목록에 client_fd 추가
```

클라이언트 연결 요청이 들어오면 `accept()`로 수락하고, 새 client fd를 non-blocking으로 만든다.

---

## 14.3 요청 읽기

```text
poll에서 client_fd가 POLLIN
recv 또는 read
HTTP request buffer에 저장
요청 파싱
```

HTTP 요청은 한 번에 다 들어오지 않을 수 있으므로 클라이언트별 버퍼에 누적해야 한다.

---

## 14.4 응답 생성

```text
stat
access
open
read
opendir / readdir / closedir
```

요청 URI에 해당하는 파일이나 디렉터리를 확인하고 응답을 만든다.

---

## 14.5 응답 전송

```text
poll에서 client_fd가 POLLOUT
send 또는 write
부분 전송 처리
전송 완료 후 close 또는 keep-alive 유지
```

응답이 한 번에 모두 전송되지 않을 수 있으므로 write buffer를 관리해야 한다.

---

## 14.6 CGI 처리

```text
pipe
fork
dup2
chdir
execve
read/write
waitpid
kill
```

CGI 실행 시 서버와 CGI 프로세스 사이에 파이프를 연결하고, 자식 프로세스에서 CGI 프로그램을 실행한다.

---

# 15. 함수별 한 줄 요약표


| 함수               | 역할                        |
| ---------------- | ------------------------- |
| `execve`         | 다른 프로그램 실행                |
| `pipe`           | 프로세스 간 통신용 파이프 생성         |
| `strerror`       | errno를 문자열로 변환            |
| `gai_strerror`   | getaddrinfo 에러를 문자열로 변환   |
| `errno`          | 시스템 콜 실패 이유 저장            |
| `dup`            | fd 복사                     |
| `dup2`           | fd를 원하는 번호로 복사            |
| `fork`           | 자식 프로세스 생성                |
| `socketpair`     | 연결된 소켓 쌍 생성               |
| `htons`          | 16비트 값을 네트워크 바이트 순서로 변환   |
| `htonl`          | 32비트 값을 네트워크 바이트 순서로 변환   |
| `ntohs`          | 16비트 값을 호스트 바이트 순서로 변환    |
| `ntohl`          | 32비트 값을 호스트 바이트 순서로 변환    |
| `select`         | 여러 fd 이벤트 감시              |
| `poll`           | 여러 fd 이벤트 감시              |
| `epoll_create`   | Linux epoll 생성            |
| `epoll_ctl`      | epoll 감시 fd 등록/수정/삭제      |
| `epoll_wait`     | epoll 이벤트 대기              |
| `kqueue`         | BSD/macOS 이벤트 큐 생성        |
| `kevent`         | kqueue 이벤트 등록/대기          |
| `socket`         | 소켓 생성                     |
| `accept`         | 클라이언트 연결 수락               |
| `listen`         | 서버 소켓 대기 상태 전환            |
| `send`           | 소켓으로 데이터 전송               |
| `recv`           | 소켓에서 데이터 수신               |
| `chdir`          | 현재 작업 디렉터리 변경             |
| `bind`           | 소켓에 IP/포트 연결              |
| `connect`        | 서버에 연결 요청                 |
| `getaddrinfo`    | 주소 정보 생성                  |
| `freeaddrinfo`   | getaddrinfo 결과 해제         |
| `setsockopt`     | 소켓 옵션 설정                  |
| `getsockname`    | 소켓 주소 정보 확인               |
| `getprotobyname` | 프로토콜 정보 확인                |
| `fcntl`          | fd 속성 변경, non-blocking 설정 |
| `close`          | fd 닫기                     |
| `read`           | fd에서 읽기                   |
| `write`          | fd에 쓰기                    |
| `waitpid`        | 자식 프로세스 종료 확인             |
| `kill`           | 프로세스에 시그널 전송              |
| `signal`         | 시그널 처리 방식 설정              |
| `access`         | 파일 접근 가능 여부 확인            |
| `stat`           | 파일/디렉터리 정보 확인             |
| `open`           | 파일 열기                     |
| `opendir`        | 디렉터리 열기                   |
| `readdir`        | 디렉터리 항목 읽기                |
| `closedir`       | 디렉터리 닫기                   |


---

# 16. webserv에서 특히 중요한 함수 TOP 10


| 중요도 | 함수                                  | 이유                   |
| --- | ----------------------------------- | -------------------- |
| 1   | `poll`                              | non-blocking I/O의 중심 |
| 2   | `socket`                            | 서버 소켓 생성             |
| 3   | `bind`                              | 포트 연결                |
| 4   | `listen`                            | 연결 대기                |
| 5   | `accept`                            | 클라이언트 연결 수락          |
| 6   | `fcntl`                             | non-blocking 설정      |
| 7   | `recv` / `read`                     | 요청 읽기                |
| 8   | `send` / `write`                    | 응답 보내기               |
| 9   | `open` / `stat` / `access`          | 정적 파일 처리             |
| 10  | `fork` / `execve` / `pipe` / `dup2` | CGI 처리               |


---

# 17. 핵심 정리

webserv 관점에서 함수들은 다음과 같이 나눌 수 있다.

```text
서버 소켓 생성:
socket, setsockopt, bind, listen

이벤트 감시:
poll, select, epoll, kqueue

클라이언트 처리:
accept, recv, send, close

non-blocking 설정:
fcntl

파일 처리:
open, read, write, access, stat

디렉터리 처리:
opendir, readdir, closedir

CGI 처리:
pipe, fork, dup2, execve, waitpid, kill

주소 변환:
htons, htonl, ntohs, ntohl, getaddrinfo

에러 처리:
errno, strerror, gai_strerror
```

---

# 18. webserv 구현 시 가장 중요한 사고방식

```text
소켓은 blocking 되면 안 된다.
read/write/send/recv는 poll이 허락한 fd에 대해서만 수행한다.
한 번의 recv로 요청이 다 온다고 가정하지 않는다.
한 번의 send로 응답이 다 나간다고 가정하지 않는다.
클라이언트마다 읽기 버퍼와 쓰기 버퍼를 따로 관리한다.
CGI pipe도 blocking될 수 있으므로 non-blocking + poll 기반으로 관리해야 한다.
```

---

# 19. DELETE 메서드 관련 주의사항

일반적으로 파일 삭제에는 다음 함수가 사용된다.

```c
unlink(path);
remove(path);
```

하지만 현재 제공된 허용 함수 목록에는 `unlink()`와 `remove()`가 없다.

따라서 이 목록이 실제 subject 기준으로 정확하다면, 일반적인 방식의 파일 삭제 구현은 제한된다.

반드시 subject PDF의 허용 함수 목록을 다시 확인해야 한다.

허용 함수 목록에 `unlink()`가 있다면 DELETE는 보통 다음 흐름으로 구현한다.

```text
1. 요청 URI를 실제 파일 경로로 변환
2. stat으로 파일 존재 여부 확인
3. 디렉터리인지 일반 파일인지 확인
4. 권한 확인
5. unlink(path) 호출
6. 성공 시 204 No Content 또는 200 OK 반환
7. 실패 시 403, 404, 500 등 반환
```

하지만 `unlink()`가 없다면 위 방식은 사용할 수 없다.

---

# 20. 추천 구현 우선순위

webserv를 처음 구현할 때는 다음 순서로 접근하는 것이 좋다.

```text
1. config 파싱
2. socket 생성
3. bind / listen
4. fcntl로 non-blocking 설정
5. poll 루프 구성
6. accept 처리
7. client별 request buffer 관리
8. HTTP request 파싱
9. GET 정적 파일 응답
10. POST body 처리
11. DELETE 처리
12. autoindex
13. error page
14. CGI pipe / fork / execve 처리
15. keep-alive 처리
16. timeout / client disconnect 처리
```

---

# 21. 결론

이 함수 목록은 단순히 외워야 하는 목록이 아니라, webserv의 구조를 이루는 핵심 도구들이다.

가장 중요한 축은 다음 네 가지이다.

```text
1. socket 기반 서버 생성
2. poll 기반 non-blocking I/O
3. 파일 시스템을 이용한 HTTP 응답 생성
4. fork / execve / pipe 기반 CGI 처리
```

특히 42 webserv에서는 다음 규칙을 항상 기억해야 한다.

```text
read/write 하기 전에 poll로 가능한 상태인지 확인한다.
한 클라이언트 때문에 서버 전체가 멈추면 안 된다.
요청과 응답은 항상 부분적으로 처리될 수 있다고 가정한다.
```

