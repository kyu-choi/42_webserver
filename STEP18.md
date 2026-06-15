# STEP 18 - 통합 테스트와 Stress Test 작성

## 이 단계의 목표

Mandatory 기능과 안정성을 한 명령으로 반복 검증할 수 있는 자동 테스트를 만든다.

수동 테스트만으로는 수정할 때마다 발생하는 회귀를 찾기 어렵다.

## 선행 조건

- [ ] [STEP17.md](STEP17.md)가 완료되었다.
- [ ] 모든 Mandatory 기능이 개별 수동 테스트에서 동작한다.
- [ ] 평가용 `config/default.conf`와 demo 파일이 있다.

## 만들 파일 예시

```text
tests/integration.sh
tests/malformed.py
tests/chunked.py
tests/slow_client.py
tests/cgi_nonblocking.py
tests/stress.py
```

## 테스트 작성 원칙

- 테스트마다 예상 status, header, body 또는 파일 결과를 명시한다.
- 실패 시 어떤 기능이 실패했는지 바로 알 수 있게 출력한다.
- 테스트가 생성한 업로드 파일 등은 다음 실행에 영향을 주지 않게 정리한다.
- 한 테스트 실패 후에도 가능한 나머지 테스트 결과를 확인한다.
- 서버 시작과 종료를 자동화할 경우 pid와 종료 처리를 안전하게 관리한다.

## 사용자가 해야 할 일

### 1. 빌드 테스트

- [ ] `make re`가 성공하는지 검사한다.
- [ ] 그 뒤 `make`가 불필요한 relink를 하지 않는지 검사한다.
- [ ] 실행 파일 이름이 `webserv`인지 검사한다.
- [ ] C++98 compiler flags가 적용되는지 확인한다.

### 2. config 테스트

정상:

- [ ] 기본 config.
- [ ] 여러 port config.
- [ ] 모든 directive를 포함한 config.

오류:

- [ ] 존재하지 않는 config.
- [ ] semicolon 누락.
- [ ] 닫히지 않은 block.
- [ ] 잘못된 port.
- [ ] 잘못된 body size.
- [ ] 알 수 없는 directive.
- [ ] upload store 없는 upload on.

기대:

```text
정상 config → 서버 시작
오류 config → listen 전에 명확히 종료
```

### 3. 정적 사이트와 여러 port 테스트

- [ ] `8080`과 `8081`이 서로 다른 body를 제공한다.
- [ ] HTML, CSS, 이미지 status와 body를 검사한다.
- [ ] binary 응답을 원본과 `cmp`한다.
- [ ] 없는 파일은 `404`인지 검사한다.
- [ ] traversal 요청이 root 밖 파일을 제공하지 않는지 검사한다.

### 4. route 기능 테스트

- [ ] longest-prefix location 선택.
- [ ] redirect status와 `Location`.
- [ ] method 제한 `405`와 `Allow`.
- [ ] 미구현 method `501`.
- [ ] custom/default error page.
- [ ] index 우선순위.
- [ ] autoindex on/off.

### 5. body와 upload 테스트

- [ ] Content-Length body 정상 처리.
- [ ] body limit 경계값.
- [ ] body limit 1 byte 초과 `413`.
- [ ] multipart text upload.
- [ ] multipart binary upload.
- [ ] upload 후 원본 byte 비교.
- [ ] overwrite status 검사.
- [ ] 악성 filename 거절.

### 6. DELETE 테스트

- [ ] 업로드한 파일 삭제 성공.
- [ ] 삭제 후 파일이 실제로 없음.
- [ ] 두 번째 삭제 `404`.
- [ ] directory 삭제 거절.
- [ ] traversal 삭제 거절.

### 7. chunked 테스트

- [ ] 여러 chunk의 정상 decoded body.
- [ ] 작은 chunk 다수.
- [ ] decoded body limit 경계.
- [ ] malformed chunk size.
- [ ] CRLF 누락.
- [ ] Content-Length와 chunked 충돌.

### 8. CGI 테스트

- [ ] GET query 전달.
- [ ] POST body 전달.
- [ ] chunked decoded body 전달.
- [ ] CGI output Content-Length 없음.
- [ ] CGI timeout.
- [ ] CGI 실패.
- [ ] CGI 실행 중 다른 정적 GET이 즉시 응답.
- [ ] client disconnect 후 child 정리.

### 9. malformed와 slow client 테스트

- [ ] request line 오류.
- [ ] Host 누락.
- [ ] header 오류.
- [ ] Content-Length 오류.
- [ ] header를 한 byte씩 전송.
- [ ] body를 한 byte씩 전송.
- [ ] 요청 중간 disconnect.
- [ ] 응답을 읽지 않는 client.

각 테스트 중 다른 정상 GET이 처리되는지 확인한다.

### 10. stress test

- [ ] 동시에 100개 이상 정적 GET.
- [ ] 여러 port에 동시 요청.
- [ ] 큰 파일 동시 다운로드.
- [ ] 정상 요청과 malformed 요청 혼합.
- [ ] CGI와 정적 요청 혼합.
- [ ] 반복 실행 후 서버 생존 확인.

예상 검증:

```text
모든 정상 요청의 status/body가 예상과 일치
서버 crash 없음
요청이 무한 대기하지 않음
테스트 후 새 요청 정상 처리
```

### 11. Valgrind와 fd 검사

- [ ] 통합 테스트 핵심 경로를 Valgrind 서버에 실행한다.
- [ ] definite leak을 해결한다.
- [ ] 종료 후 열린 fd가 남는지 검사한다.
- [ ] zombie process가 없는지 검사한다.

## 수동 NGINX 비교

다음을 NGINX와 비교한다.

```text
404
405
redirect
directory/index/autoindex
Content-Length
Content-Type
Connection
```

완전히 같은 raw response가 목표는 아니지만 status와 핵심 행동이 합리적이어야 한다.

## 자주 발생하는 문제

### 테스트 순서에 따라 결과가 달라짐

원인:

- 이전 upload/delete 결과를 정리하지 않음.
- port/process가 남아 있음.
- 테스트 데이터가 공유됨.

### stress test는 통과하지만 일부 response가 잘못됨

단순 성공 개수만 세지 말고 status, Content-Length, body도 검증한다.

### 서버 시작 대기 없이 요청을 보냄

서버가 listen 준비 상태가 될 때까지 제한 시간 내에서 확인하는 로직을 둔다.

## 완료 조건

- [ ] 한 명령으로 주요 Mandatory 기능을 검사한다.
- [ ] 테스트 실패 원인을 명확히 출력한다.
- [ ] malformed/slow/disconnect 테스트가 있다.
- [ ] CGI non-blocking과 timeout 테스트가 있다.
- [ ] stress test 후에도 서버가 동작한다.
- [ ] memory/fd/child leak 검사를 완료했다.
- [ ] NGINX 비교를 통해 주요 동작을 점검했다.

다음 단계: [STEP19.md](STEP19.md)
