# STEP 04 구현 결과

## 목표

`STEP04.md`의 목표는 client별 입력 buffer, 출력 buffer, send offset, 상태, 마지막 활동 시간을 독립적으로 저장하여 partial recv/send를 안전하게 처리하는 것이다.

Step03에서는 `EventLoop` 내부 임시 `Client` struct가 request/response buffer와 전송 위치를 직접 들고 있었다. 이번 단계에서는 이를 별도 `Client` 클래스로 분리했다.

## 생성/수정한 파일

| 경로 | 구현 내용 |
|---|---|
| `include/webserv/Client.hpp` | client fd, listen fd, input/output buffer, send offset, 상태, 마지막 활동 시간 선언 |
| `src/core/Client.cpp` | partial I/O helper, 상태 전환, poll event 결정 구현 |
| `include/webserv/EventLoop.hpp` | 내부 임시 `Client` struct 제거, `webserv::Client` 사용 |
| `src/core/EventLoop.cpp` | client map에 `Client` 객체 저장, partial recv/send를 `Client` API로 처리 |
| `Makefile` | `src/core/Client.cpp`를 빌드 대상에 추가 |

## Client 클래스 구현 내용

`Client`는 fd 자체를 닫지는 않는다. fd close의 소유권은 여전히 `EventLoop::closeClient()`에 있다. `Client`는 한 client 연결의 상태 데이터만 저장한다.

저장하는 값은 다음과 같다.

```text
_fd
-> client socket fd

_listenFd
-> 이 client를 accept한 listen fd

_inputBuffer
-> recv로 받은 byte 누적

_outputBuffer
-> send할 전체 response

_sendOffset
-> response 중 이미 보낸 위치

_state
-> CLIENT_READING_HEADERS, CLIENT_PROCESSING_REQUEST, CLIENT_WRITING_RESPONSE, CLIENT_CLOSING

_lastActivity
-> 마지막 recv/send/state 변경 시간

_closing
-> close 진행 여부
```

## partial recv 처리

`Client::appendInput()`은 `recv()`가 실제로 반환한 byte 수만큼만 append한다.

```cpp
_inputBuffer.append(data, size);
```

`strlen()`이나 null-terminated C string을 기준으로 처리하지 않는다. 따라서 binary byte나 조각난 request에도 안전한 방향이다.

현재 단계에서는 아직 HTTP parser가 없으므로, request header 끝인 `\r\n\r\n`이 보이면 request가 준비된 것으로 보고 고정 response를 준비한다.

```text
"G"
"ET / HT"
"TP/1.1\r\n"
"Host: localhost\r\n\r\n"
```

이렇게 여러 조각으로 들어와도 `_inputBuffer`에 누적된 뒤 `hasCompleteHeaders()`가 true가 된다.

input buffer는 임시 정책으로 1MB를 넘으면 client를 닫는다. 이후 `client_max_body_size`와 request parser가 연결되면 `413 Payload Too Large` 응답으로 바꿀 수 있다.

## partial send 처리

`Client::setOutput()`은 보낼 전체 response를 `_outputBuffer`에 저장하고 `_sendOffset`을 0으로 초기화한다.

`send()`할 때는 항상 다음 위치부터 보낸다.

```text
send 시작 위치 = outputBuffer + sendOffset
남은 크기 = outputBuffer.size() - sendOffset
```

전송에 성공한 byte 수만큼 `advanceSendOffset()`으로 offset을 증가시킨다.

```text
send 일부 성공
-> sendOffset 증가
-> 아직 남은 데이터가 있으면 다음 POLLOUT에서 계속 전송

send 완료
-> CLIENT_CLOSING
-> closeClient()
```

## 상태에 따른 poll event

`Client::desiredPollEvents()`가 현재 상태에 맞는 event를 반환한다.

```text
CLIENT_READING_HEADERS / CLIENT_READING_BODY
-> POLLIN

CLIENT_WRITING_RESPONSE + 남은 output 있음
-> POLLOUT

CLIENT_CLOSING
-> event 없음
```

이제 `EventLoop`는 client 상태를 직접 추측하지 않고, client가 원하는 poll event를 물어본 뒤 `updateEvents()`를 호출한다.

## 기존 코드에서 바뀐 점

Step03에서는 `EventLoop` 안에 다음 임시 구조가 있었다.

```cpp
struct Client {
    std::string requestBuffer;
    std::string responseBuffer;
    std::size_t bytesSent;
    bool responseReady;
};
```

Step04에서는 이 구조를 제거하고 별도 `Client` 클래스로 옮겼다.

```text
Step03:
EventLoop가 client buffer와 send offset을 직접 수정

Step04:
Client가 buffer, send offset, state, lastActivity를 관리
EventLoop는 poll event와 fd lifecycle을 관리
```

## 이번 단계의 한계

- 아직 HTTP request parser는 없다.
- body와 chunked request는 아직 처리하지 않는다.
- input buffer 1MB 초과 시 현재는 연결을 닫는다.
- timeout은 `lastActivity`만 저장하고 아직 적용하지 않는다.
- response는 여전히 고정 `Hello World!`이다.

## 완료 체크

- [x] client별 input/output buffer가 독립적이다.
- [x] partial recv를 `append(data, receivedByteCount)`로 누적한다.
- [x] partial send가 send offset을 기준으로 동작한다.
- [x] client 상태에 따라 poll event를 변경한다.
- [x] 느린 client가 다른 client를 막지 않는다.
- [x] 조각난 request가 하나의 input buffer에 누적된 뒤 응답된다.
- [x] 중간 disconnect 후 다른 요청이 정상 처리된다.

다음 단계는 `STEP05.md`의 HTTP Request Parser 구현이다.
