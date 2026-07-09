# STEP 14 구현 결과

## 목표

`STEP14.md`의 목표는 DELETE가 허용된 route 안의 regular file만 안전하게 삭제하고 정확한 status를 반환하는 것이다.

이번 단계에서는 `/delete/...` location을 upload store에 매핑하고, Router가 계산한 안전한 filesystem path를 그대로 사용해 삭제하도록 구현했다.

```text
DELETE request
-> Router로 server/location/effective config 선택
-> route별 method 검사
-> trailing slash URI 거절
-> DeleteHandler가 stat()으로 target 검사
-> regular file이면 std::remove()
-> 성공 시 204 No Content
```

## 생성/수정한 파일

| 경로 | 구현 내용 |
|---|---|
| `include/webserv/DeleteHandler.hpp` | DELETE handler 인터페이스 추가 |
| `src/handlers/DeleteHandler.cpp` | regular file 삭제, status mapping 구현 |
| `src/core/EventLoop.cpp` | DELETE method를 DeleteHandler로 연결 |
| `src/main.cpp` | 실행 메시지를 STEP14에 맞게 변경 |
| `Makefile` | `DeleteHandler.cpp` 빌드 대상 추가 |
| `config/step14.conf` | upload/delete 검증용 config 추가 |
| `STEP14_RESULT.md` | 구현 결과 문서 |

## Method와 Route 검사

DELETE method 자체는 기존 HTTP method table에 이미 포함되어 있다.

EventLoop의 기존 method 제한 흐름을 그대로 사용한다.

```text
DELETE가 route methods에 없음
-> 405 Method Not Allowed + Allow header

DELETE가 route methods에 있음
-> DeleteHandler 실행
```

예:

```text
DELETE /
-> location / 은 GET만 허용
-> 405, Allow: GET

DELETE /delete/file.txt
-> location /delete 는 DELETE 허용
-> DeleteHandler 실행
```

## 안전한 Path 생성

DELETE도 GET/static과 같은 Router/PathPolicy 흐름을 사용한다.

```text
query 제거
percent decode
.. traversal 검사
location prefix 매칭
root + relative path 결합
```

`config/step14.conf`에서는 다음처럼 `/delete`를 upload store에 매핑했다.

```conf
location /delete {
    methods DELETE;
    root ./www/uploads;
}
```

따라서:

```text
DELETE /delete/README.md
-> ./www/uploads/README.md
```

## Target 검사

`DeleteHandler`는 삭제 전에 `stat()`으로 target을 검사한다.

```text
relativePath empty -> 403
존재하지 않음      -> 404
directory          -> 403
regular file 아님  -> 403
regular file       -> 삭제 시도
```

삭제는 `std::remove()`를 사용한다.

실패 status:

```text
ENOENT        -> 404
EACCES/EPERM  -> 403
그 외         -> 500
```

## Trailing Slash 보안 수정

테스트 중 다음 케이스가 위험하다는 것을 발견했다.

```text
DELETE /delete/README.md/
```

기존 Router 정규화 과정에서 trailing slash가 사라지면서 regular file인 `README.md`로 매핑될 수 있었다.

수정 후 EventLoop는 DELETE 요청의 `route.uriPath`가 `/`로 끝나면 삭제하지 않고 `403 Forbidden`을 반환한다.

```text
DELETE /delete/README.md/
-> 403
-> README.md 보존
```

## Response

삭제 성공 시:

```http
HTTP/1.1 204 No Content
Content-Length: 0
```

`HttpResponse`는 이미 `204` body를 serialize하지 않도록 되어 있어서 body가 포함되지 않는다.

## 테스트 config

`config/step14.conf`:

```conf
server {
    listen 127.0.0.1:8080;
    root ./www;
    index index.html;
    client_max_body_size 10M;

    error_page 404 /errors/404.html;

    location / {
        methods GET;
    }

    location /upload {
        methods GET POST;
        upload on;
        upload_store ./www/uploads;
    }

    location /delete {
        methods DELETE;
        root ./www/uploads;
    }
}
```

## 검증 결과

빌드:

```bash
make
```

결과:

```text
성공
```

업로드 후 삭제:

```bash
curl -i -F 'file=@README.md;filename=step14-delete.txt' http://127.0.0.1:8080/upload
curl -i -X DELETE http://127.0.0.1:8080/delete/step14-delete.txt
```

결과:

```text
upload -> 201 Created
delete -> 204 No Content
```

삭제 후 파일 확인:

```bash
test -f www/uploads/step14-delete.txt && echo exists || echo missing
```

결과:

```text
missing
```

두 번째 삭제:

```bash
curl -i -X DELETE http://127.0.0.1:8080/delete/step14-delete.txt
```

결과:

```text
404 Not Found
```

method 금지:

```bash
curl -i -X DELETE http://127.0.0.1:8080/
```

결과:

```text
405 Method Not Allowed
Allow: GET
```

delete root/directory 거절:

```bash
curl -i -X DELETE http://127.0.0.1:8080/delete/
```

결과:

```text
403 Forbidden
```

encoded traversal:

```bash
curl -i -X DELETE http://127.0.0.1:8080/delete/%2e%2e/%2e%2e/README.md
```

결과:

```text
403 Forbidden
```

trailing slash file-like URI:

```bash
curl -i -X DELETE http://127.0.0.1:8080/delete/README.md/
```

결과:

```text
403 Forbidden
README.md 보존
```

정상 DELETE 재확인:

```bash
curl -i -X DELETE http://127.0.0.1:8080/delete/README.md
```

결과:

```text
204 No Content
```

## 이번 단계의 한계

- directory 삭제는 지원하지 않는다.
- recursive delete는 지원하지 않는다.
- symlink는 `stat()` 결과 기준으로 판단한다.
- DELETE response body는 제공하지 않는다.

## 완료 체크

- [x] route별 DELETE 허용 여부를 검사한다.
- [x] 안전한 실제 path를 사용한다.
- [x] regular file만 삭제한다.
- [x] root와 directory를 삭제하지 않는다.
- [x] 성공 시 정확한 `204`를 반환한다.
- [x] traversal로 root 밖 파일을 삭제할 수 없다.
- [x] 삭제 실패 후 서버가 계속 동작한다.

다음 단계는 `STEP15.md`의 chunked request body 구현이다.
