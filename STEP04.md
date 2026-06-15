# STEP 04 - Client 상태와 Partial I/O 구현

## 이 단계의 목표

각 client의 입력, 출력, 전송 위치, 상태를 독립적으로 저장하여 요청과 응답이 여러 조각으로 나뉘어도 처리할 수 있게 한다.

TCP는 메시지 경계를 보장하지 않는다.

```text
요청 하나가 여러 recv로 나뉠 수 있다.
요청 여러 개가 recv 한 번에 들어올 수 있다.
response 전체가 send 한 번에 전송되지 않을 수 있다.
```

## 선행 조건

- [ ] [STEP03.md](STEP03.md)의 poll event loop가 동작한다.
- [ ] 여러 client fd를 동시에 관리할 수 있다.
- [ ] fd를 등록하고 제거하는 공통 함수가 있다.

## 만들 파일 예시

```text
include/Client.hpp
src/core/Client.cpp
```

## Client에 저장할 정보

```text
client fd
들어온 listen fd
input buffer
output buffer
send offset
현재 상태
마지막 활동 시간
연결 종료 여부
추후 사용할 request/parser/CGI 정보
```

상태 예시:

```cpp
enum ClientState {
    READING_REQUEST,
    PROCESSING_REQUEST,
    WRITING_RESPONSE,
    CLOSING
};
```

## 사용자가 해야 할 일

### 1. Client 클래스 또는 구조체 작성

- [ ] 생성 시 fd와 listen fd를 저장한다.
- [ ] input/output buffer를 빈 상태로 초기화한다.
- [ ] send offset을 0으로 초기화한다.
- [ ] 초기 상태를 `READING_REQUEST`로 정한다.
- [ ] 마지막 활동 시간을 저장한다.
- [ ] 복사와 소유권 정책을 정한다.

### 2. client 저장소 만들기

- [ ] client fd로 Client를 찾을 수 있는 map을 만든다.
- [ ] accept 성공 시 Client를 생성한다.
- [ ] fd close 시 Client도 제거한다.
- [ ] poll 목록과 client 저장소가 항상 일치하도록 한다.

### 3. partial recv 처리

- [ ] `recv()`가 반환한 실제 byte 수만 append한다.
- [ ] C 문자열 종료 문자를 기준으로 처리하지 않는다.
- [ ] 받은 데이터가 요청 전체라고 가정하지 않는다.
- [ ] input buffer 최대 크기 정책을 정한다.
- [ ] read 성공 시 마지막 활동 시간을 갱신한다.

올바른 개념:

```cpp
inputBuffer.append(buffer, receivedByteCount);
```

잘못된 개념:

```text
strlen(buffer) 사용
recv 한 번 후 바로 요청 처리
```

### 4. partial send 처리

- [ ] output buffer에 보낼 전체 response를 저장한다.
- [ ] `send()` 시작 위치를 `sendOffset`으로 계산한다.
- [ ] 실제 전송된 byte 수만큼 offset을 증가시킨다.
- [ ] 아직 남은 데이터가 있으면 다음 `POLLOUT`을 기다린다.
- [ ] 전송 완료 후에만 client를 닫거나 다음 상태로 이동한다.

개념:

```text
남은 데이터 = output buffer size - send offset
send 대상 시작 = output buffer + send offset
```

### 5. 상태 전환 구현

기본 흐름:

```text
READING_REQUEST
→ 요청 처리 조건 충족
PROCESSING_REQUEST
→ response 준비 완료
WRITING_RESPONSE
→ 전송 완료
CLOSING
```

- [ ] 상태별로 감시할 poll event를 정한다.
- [ ] 읽는 상태에서 불필요한 `POLLOUT`을 끈다.
- [ ] 쓰는 상태에서 필요한 `POLLOUT`을 켠다.
- [ ] close 상태에서 더 이상 I/O하지 않는다.

### 6. 공통 cleanup 함수 작성

cleanup에서 해야 할 일:

- [ ] poll 목록에서 fd 제거.
- [ ] client map에서 Client 제거.
- [ ] fd close.
- [ ] 이후 추가될 CGI 관계 정리 위치 확보.
- [ ] 이미 제거된 client에도 안전하게 동작.

## 테스트 작성

### 요청을 조각내어 전송

Python socket 테스트를 작성해 다음처럼 보낸다.

```text
"G"
"ET / HT"
"TP/1.1\r\n"
"Host: localhost\r\n\r\n"
```

### 느리게 요청 전송

```text
1초마다 한 byte 전송
동시에 다른 curl 요청 실행
```

### 큰 response와 느린 reader

```text
큰 output buffer 준비
client가 response를 천천히 읽음
다른 client 요청이 정상 처리되는지 확인
```

### 중간 disconnect

```text
request 전송 중 disconnect
response 전송 중 disconnect
그 뒤 서버 정상 동작 확인
```

## 자주 발생하는 문제

### output buffer는 있는데 전송되지 않음

확인:

- client poll event에 `POLLOUT`이 등록되었는가?
- 상태가 `WRITING_RESPONSE`로 변경되었는가?

### response 일부가 중복됨

확인:

- send offset을 실제 전송량만큼 증가시키는가?
- 매번 output buffer 처음부터 전송하지 않는가?

### binary 데이터가 잘림

확인:

- `strlen()`을 사용하지 않는가?
- byte 수와 `std::string::size()`를 사용하는가?

### close 후 crash

확인:

- client를 제거한 뒤 reference나 iterator를 다시 사용하지 않는가?
- poll vector 삭제 후 index를 안전하게 처리하는가?

## 완료 조건

- [ ] client별 input/output buffer가 독립적이다.
- [ ] partial recv를 누적한다.
- [ ] partial send와 send offset이 동작한다.
- [ ] 상태에 따라 poll event를 변경한다.
- [ ] 느린 client가 다른 client를 막지 않는다.
- [ ] 중간 disconnect 후 fd와 Client가 정상 정리된다.

다음 단계: [STEP05.md](STEP05.md)
