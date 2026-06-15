# STEP 20 - Mandatory 완료 후 Bonus 구현

## 이 단계의 목표

Mandatory 기능과 안정성을 손상시키지 않으면서 Bonus 기능을 추가한다.

Bonus는 Mandatory가 완전히 정상일 때만 평가된다. Mandatory test가 하나라도 실패한다면 이 단계로 넘어가지 않는다.

## 선행 조건

- [ ] [STEP19.md](STEP19.md)가 완료되었다.
- [ ] 모든 Mandatory 통합 테스트가 반복해서 통과한다.
- [ ] stress 및 Valgrind 검사에서 문제가 없다.
- [ ] 평가 시 모든 Mandatory 구조를 설명할 수 있다.

## Bonus 후보

```text
Cookie와 session management
여러 CGI type
```

## 사용자가 해야 할 일

### 1. Bonus 시작 전 기준 결과 저장

- [ ] Mandatory 통합 테스트 결과를 기록한다.
- [ ] stress test 결과를 기록한다.
- [ ] Valgrind 결과를 기록한다.
- [ ] Bonus 변경 전 동작을 재현할 수 있게 한다.

Bonus 구현 후 같은 결과와 비교한다.

### 2. Cookie parsing 구현

- [ ] request의 `Cookie` header를 파싱한다.
- [ ] 여러 cookie를 name/value로 저장한다.
- [ ] 잘못된 cookie를 안전하게 처리한다.
- [ ] response에 `Set-Cookie` header를 추가할 수 있게 한다.
- [ ] cookie 값 검증과 escaping 정책을 정한다.

### 3. Session management 구현

- [ ] 안전한 session id 생성 방식을 정한다.
- [ ] session id를 cookie로 전달한다.
- [ ] session id별 data 저장소를 만든다.
- [ ] 마지막 접근 시간을 기록한다.
- [ ] TTL 이후 session을 제거한다.
- [ ] 최대 session 개수 또는 memory 제한을 둔다.
- [ ] session이 필요한 route에서만 생성한다.
- [ ] 모든 응답에서 불필요하게 session을 만들지 않는다.

session 무제한 증가는 장시간 stress에서 서버 안정성을 해칠 수 있다.

### 4. Session 예제 제공

- [ ] 처음 요청 시 session 생성.
- [ ] 이후 같은 cookie 요청에서 값 유지.
- [ ] 방문 횟수 등의 간단한 상태 예제.
- [ ] 만료 후 새 session 생성.
- [ ] 브라우저와 curl로 확인 가능한 demo route.

curl 예:

```bash
curl -c /tmp/webserv-cookie http://127.0.0.1:8080/session
curl -b /tmp/webserv-cookie http://127.0.0.1:8080/session
```

### 5. 여러 CGI type 지원

- [ ] extension별 interpreter map을 사용한다.
- [ ] `.py`와 다른 CGI type을 추가한다.
- [ ] 각 interpreter 존재 여부와 실패를 처리한다.
- [ ] 모든 CGI type이 같은 non-blocking pipe 구조를 사용한다.
- [ ] CGI type별 GET/POST/timeout을 테스트한다.

예:

```conf
cgi .py /usr/bin/python3;
cgi .php /usr/bin/php-cgi;
```

### 6. Bonus 자동 테스트 추가

- [ ] cookie 저장/전송 테스트.
- [ ] session 유지 테스트.
- [ ] session 만료 테스트.
- [ ] session 수 제한 테스트.
- [ ] 여러 CGI type 테스트.
- [ ] Bonus stress test.

### 7. Mandatory 회귀 검사

Bonus 기능을 추가할 때마다:

```bash
make re
./tests/integration.sh
```

- [ ] 모든 Mandatory 테스트가 그대로 통과한다.
- [ ] non-blocking 규칙을 위반하지 않는다.
- [ ] fd/memory/child leak이 새로 생기지 않는다.
- [ ] Mandatory config만 사용해도 정상 동작한다.

## 자주 발생하는 문제

### 모든 요청에서 session 생성

문제:

- cookie 없는 stress 요청마다 session map이 증가한다.

해결:

- session이 필요한 route에서만 생성하고 TTL/최대 개수를 적용한다.

### 여러 CGI type 구현 때문에 CGI 코드가 중복됨

해결:

- 공통 CGI 실행 흐름은 유지하고 interpreter 선택만 extension mapping으로 분리한다.

### Bonus 추가 후 Mandatory가 실패함

Bonus 평가보다 Mandatory 성공이 우선이다. 회귀 원인을 수정하거나 Bonus 변경을 제거한다.

## 완료 조건

- [ ] Bonus 시작 전 모든 Mandatory가 완성되었다.
- [ ] cookie/session 예제가 동작한다.
- [ ] session TTL과 자원 제한이 있다.
- [ ] 여러 CGI type이 공통 non-blocking 구조로 동작한다.
- [ ] Bonus 자동 테스트가 있다.
- [ ] Bonus 후에도 모든 Mandatory 테스트가 통과한다.
- [ ] stress와 Valgrind에서 새로운 문제가 없다.

전체 진행 문서: [STEP.md](STEP.md)
