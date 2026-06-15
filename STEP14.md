# STEP 14 - DELETE 구현

## 이 단계의 목표

DELETE가 허용된 route 안의 파일만 안전하게 삭제하고 정확한 status를 반환한다.

## 선행 조건

- [ ] [STEP13.md](STEP13.md)가 완료되었다.
- [ ] 안전한 URI-to-filesystem path helper가 있다.
- [ ] route별 method 제한이 동작한다.

## 만들 파일 예시

```text
include/DeleteHandler.hpp
src/handlers/DeleteHandler.cpp
```

## 사용자가 해야 할 일

### 1. method와 route 검사

- [ ] 서버가 DELETE를 지원하는 method로 인식한다.
- [ ] selected location에서 DELETE가 허용되었는지 검사한다.
- [ ] 금지된 route에서는 `405`와 `Allow` header를 반환한다.
- [ ] DELETE 허용 범위를 upload/delete용 route로 제한하는 방식을 검토한다.

### 2. 안전한 실제 path 생성

- [ ] query string을 제거한다.
- [ ] URI path를 정규화한다.
- [ ] `..`, 절대 경로, encoded traversal을 검사한다.
- [ ] selected root와 결합한다.
- [ ] 최종 path가 허용 root 내부인지 확인한다.

GET과 다른 path 규칙을 별도로 만들지 말고 공통 helper를 사용한다.

### 3. target 상태 확인

- [ ] `stat()`으로 target 존재 여부를 확인한다.
- [ ] 존재하지 않으면 `404`를 반환한다.
- [ ] regular file인지 확인한다.
- [ ] directory 삭제를 거절한다.
- [ ] root 자체 삭제를 거절한다.
- [ ] 권한이 없으면 `403` 등 적절한 status를 반환한다.

### 4. 파일 삭제

- [ ] 평가 환경에서 허용되는 삭제 함수를 확인한다.
- [ ] C++98 표준 라이브러리 `std::remove()` 사용 가능 여부를 평가 기준과 대조한다.
- [ ] 삭제 성공 여부를 확인한다.
- [ ] 실패 원인을 status로 변환한다.

공식 external function 목록에 `unlink()`가 명시되어 있지 않으므로 임의로 사용하지 않는다.

### 5. response 생성

권장:

| 상황 | Status |
|---|---:|
| 삭제 성공 | `204 No Content` |
| 파일 없음 | `404 Not Found` |
| method 금지 | `405 Method Not Allowed` |
| 접근/권한 거절 | `403 Forbidden` |
| directory 삭제 거절 | `403` 또는 `409` |
| 내부 삭제 실패 | `500 Internal Server Error` |

- [ ] `204` response에는 body를 넣지 않는다.
- [ ] Content-Length 정책을 일관되게 적용한다.

## 실행 및 검증

먼저 업로드:

```bash
curl -i -F file=@README.md http://127.0.0.1:8080/upload
```

삭제:

```bash
curl -i -X DELETE http://127.0.0.1:8080/delete/README.md
curl -i -X DELETE http://127.0.0.1:8080/delete/README.md
```

검사:

```text
첫 요청 → 204, 파일 삭제됨
두 번째 요청 → 404
```

보안 테스트:

```text
DELETE /
DELETE /delete/
DELETE /delete/../../important.txt
encoded traversal DELETE
directory DELETE
```

## 자주 발생하는 문제

### GET traversal은 막지만 DELETE traversal은 가능함

해결:

- 모든 filesystem handler가 같은 안전한 path helper를 사용한다.

### directory가 삭제됨

해결:

- 삭제 전에 반드시 `stat()`으로 regular file 여부를 확인한다.

### 성공 response에 body가 포함됨

해결:

- `204 No Content` 규칙을 ResponseBuilder에서 강제한다.

## 완료 조건

- [ ] route별 DELETE 허용 여부를 검사한다.
- [ ] 안전한 실제 path를 사용한다.
- [ ] regular file만 삭제한다.
- [ ] root와 directory를 삭제하지 않는다.
- [ ] 성공 시 정확한 `204`를 반환한다.
- [ ] traversal로 root 밖 파일을 삭제할 수 없다.
- [ ] 삭제 실패 후 서버가 계속 동작한다.

다음 단계: [STEP15.md](STEP15.md)
