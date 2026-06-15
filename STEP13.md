# STEP 13 - POST 파일 업로드 구현

## 이 단계의 목표

upload가 허용된 route에서 raw body 또는 `multipart/form-data` 파일을 안전하게 저장한다.

## 선행 조건

- [ ] [STEP12.md](STEP12.md)가 완료되었다.
- [ ] request body가 완성된 뒤 handler가 호출된다.
- [ ] Router가 upload 허용 여부와 upload store를 제공한다.

## 만들 파일 예시

```text
include/UploadHandler.hpp
src/handlers/UploadHandler.cpp
www/upload.html
www/uploads/
```

## 사용자가 해야 할 일

### 1. 업로드 route 검사

- [ ] request method가 POST인지 확인한다.
- [ ] selected location에서 POST가 허용되는지 확인한다.
- [ ] upload가 켜져 있는지 확인한다.
- [ ] upload store 경로를 얻는다.
- [ ] upload store가 존재하는 directory인지 확인한다.
- [ ] 쓰기 가능한지 확인한다.

권장 status:

```text
POST method 금지 → 405
POST는 허용하지만 upload off → 403
upload store 접근 불가 → 403 또는 500
body 제한 초과 → 413
```

### 2. raw body 업로드 정책 구현

raw body 요청 예:

```http
POST /upload HTTP/1.1
Content-Type: application/octet-stream
Content-Length: 5

hello
```

- [ ] filename 생성 정책을 정한다.
- [ ] body를 binary-safe하게 파일에 쓴다.
- [ ] 저장 성공 여부를 확인한다.
- [ ] 생성된 resource 위치를 response에 제공할지 결정한다.

### 3. multipart boundary 추출

예:

```http
Content-Type: multipart/form-data; boundary=----abc
```

- [ ] Content-Type이 multipart인지 확인한다.
- [ ] boundary parameter를 추출한다.
- [ ] boundary가 비어 있거나 잘못되면 `400`으로 처리한다.
- [ ] 시작 boundary와 마지막 boundary 형식을 구분한다.

### 4. multipart part 파싱

각 part:

```text
boundary
part headers
빈 줄
part content
```

- [ ] boundary 기준으로 part를 분리한다.
- [ ] part header 끝을 찾는다.
- [ ] `Content-Disposition`을 파싱한다.
- [ ] `name`과 `filename` parameter를 추출한다.
- [ ] part content의 정확한 byte 범위를 계산한다.
- [ ] boundary 앞뒤 CRLF가 파일 content에 섞이지 않게 한다.
- [ ] 여러 part 처리 정책을 정한다.

### 5. filename 검증

반드시 거절하거나 안전하게 정제할 값:

```text
../file
../../.bashrc
/absolute/path
backslash 포함 경로
제어 문자
빈 filename
```

- [ ] slash와 backslash를 거절한다.
- [ ] `..` segment를 거절한다.
- [ ] 절대 경로를 거절한다.
- [ ] 제어 문자를 거절한다.
- [ ] upload store 밖 결과가 나오지 않는지 최종 검사한다.

### 6. 파일 저장

- [ ] 대상 path를 안전하게 만든다.
- [ ] 기존 파일 존재 여부를 저장 전에 확인한다.
- [ ] `open()`과 `write()`를 사용해 content를 저장한다.
- [ ] partial file write를 처리한다.
- [ ] 성공과 실패 경로에서 file fd를 닫는다.
- [ ] 저장 실패 status를 결정한다.

### 7. 정확한 성공 status

권장:

```text
새 파일 생성 → 201 Created
기존 파일 overwrite → 200 OK 또는 204 No Content
```

항상 `201`을 반환하지 않도록 저장 전 존재 여부를 확인한다.

## upload form 예시

```html
<form action="/upload" method="post" enctype="multipart/form-data">
  <input type="file" name="file">
  <button type="submit">Upload</button>
</form>
```

## 실행 및 검증

```bash
curl -i -F file=@README.md http://127.0.0.1:8080/upload
cmp README.md www/uploads/README.md
```

binary:

```bash
curl -i -F file=@www/images/logo.png http://127.0.0.1:8080/upload
cmp www/images/logo.png www/uploads/logo.png
```

보안:

```text
filename="../outside.txt"
filename="/tmp/outside.txt"
filename=""
```

## 자주 발생하는 문제

### 업로드 파일 끝에 CRLF가 추가됨

원인:

- multipart boundary 앞 CRLF까지 content로 저장함.

### binary 파일이 깨짐

원인:

- C 문자열 함수 사용.
- part content 길이 계산 오류.

### 파일은 저장되지만 status가 항상 201

해결:

- 저장 전에 대상 파일 존재 여부를 검사한다.

## 완료 조건

- [ ] upload 설정과 method를 검사한다.
- [ ] raw body 업로드 정책이 동작한다.
- [ ] multipart boundary와 part를 파싱한다.
- [ ] filename을 안전하게 검증한다.
- [ ] text와 binary 파일이 원본과 동일하게 저장된다.
- [ ] 신규 생성과 overwrite status를 구분한다.
- [ ] root/upload store 밖 파일을 생성할 수 없다.

다음 단계: [STEP14.md](STEP14.md)
