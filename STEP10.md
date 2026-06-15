# STEP 10 - Method 제한, Redirect, Custom Error 구현

## 이 단계의 목표

선택된 server/location 설정에 따라 실제 파일 처리 전에 redirect, method 제한, custom error page를 적용한다.

## 선행 조건

- [ ] [STEP09.md](STEP09.md)의 Router가 완료되었다.
- [ ] effective config를 얻을 수 있다.
- [ ] 공통 error response와 정적 파일 읽기가 동작한다.

## 요청 처리 우선순위

```text
1. server 선택
2. location 선택
3. redirect 확인
4. method 허용 여부 확인
5. body 크기 확인
6. CGI/upload/static/DELETE 처리
7. error page 적용
```

## 사용자가 해야 할 일

### 1. redirect 처리

- [ ] location의 redirect 설정 유무를 확인한다.
- [ ] redirect가 있으면 다른 handler를 호출하지 않는다.
- [ ] 설정된 status code를 사용한다.
- [ ] `Location` header를 설정한다.
- [ ] body와 Content-Length 정책을 정한다.

예:

```conf
location /old {
    return 301 /new;
}
```

기대 response:

```http
HTTP/1.1 301 Moved Permanently
Location: /new
Content-Length: 0
```

### 2. route별 method 검사

- [ ] selected location의 허용 method 목록을 얻는다.
- [ ] 현재 request method가 목록에 있는지 검사한다.
- [ ] route에서 금지된 method는 `405`로 처리한다.
- [ ] `405` response에 `Allow` header를 넣는다.
- [ ] 서버가 전혀 구현하지 않은 method는 `501`로 처리한다.

구분:

```text
DELETE를 구현하지만 현재 route에서 금지 → 405
PUT을 서버가 구현하지 않음 → 501
```

### 3. custom error page 적용

- [ ] ServerConfig의 status별 error page 경로를 조회한다.
- [ ] custom error file을 안전하게 읽는다.
- [ ] 읽기에 성공하면 원래 error status를 유지하고 body만 교체한다.
- [ ] custom error file이 없거나 읽을 수 없으면 기본 error page를 사용한다.
- [ ] custom error 처리 실패가 다시 custom error 처리로 재귀하지 않게 한다.

### 4. 에러 response 공통화

- [ ] handler가 error status만 반환할 수 있게 한다.
- [ ] 최종 response 생성 단계에서 custom/default error page를 선택한다.
- [ ] 모든 4xx/5xx에 일관되게 적용한다.
- [ ] error page의 MIME type과 Content-Length를 설정한다.

## 테스트용 config와 파일

```conf
server {
    error_page 404 /errors/404.html;

    location / {
        methods GET;
    }

    location /old {
        return 301 /;
    }
}
```

```text
www/errors/404.html
```

## 실행 및 검증

```bash
curl -i http://127.0.0.1:8080/old
curl -i -X DELETE http://127.0.0.1:8080/
curl -i -X PUT http://127.0.0.1:8080/
curl -i http://127.0.0.1:8080/not-found
```

확인:

- redirect status와 `Location`.
- `405`와 `Allow`.
- `501` 구분.
- custom 404 body.
- custom file 제거 후 기본 error page fallback.

## 자주 발생하는 문제

### redirect route에서 파일 handler까지 실행됨

해결:

- redirect를 routing 후 가장 먼저 처리하고 즉시 response를 확정한다.

### 모든 잘못된 method가 405

해결:

- 서버가 알고 있는 method인지 먼저 검사하고, route 제한과 구분한다.

### custom error page status가 200으로 변경됨

해결:

- custom 파일은 error response body일 뿐이며 원래 error status를 유지한다.

## 완료 조건

- [ ] redirect가 handler보다 먼저 처리된다.
- [ ] redirect response에 올바른 `Location`이 있다.
- [ ] route별 method 제한이 적용된다.
- [ ] `405`와 `501`을 구분한다.
- [ ] `405`에 `Allow` header가 있다.
- [ ] custom error page와 기본 fallback이 동작한다.

다음 단계: [STEP11.md](STEP11.md)
