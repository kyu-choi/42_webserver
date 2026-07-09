# STEP 13 구현 결과

## 목표

`STEP13.md`의 목표는 upload가 허용된 route에서 POST body를 안전하게 파일로 저장하는 것이다.

이번 단계에서는 다음 업로드 방식을 구현했다.

```text
raw body upload
multipart/form-data file upload
```

처리 흐름:

```text
Router effective config 확인
-> POST method 허용 여부는 기존 EventLoop method 검사에서 처리
-> upload on/off 확인
-> upload_store directory 확인
-> Content-Type에 따라 raw 또는 multipart 처리
-> filename 검증
-> open/write/close로 binary-safe 저장
-> 새 파일이면 201 Created
-> overwrite면 200 OK
```

## 생성/수정한 파일

| 경로 | 구현 내용 |
|---|---|
| `include/webserv/UploadHandler.hpp` | 업로드 handler 인터페이스 추가 |
| `src/handlers/UploadHandler.cpp` | raw/multipart upload 저장, filename 검증, 201/200 구분 구현 |
| `src/core/EventLoop.cpp` | POST route가 upload on이면 UploadHandler로 연결 |
| `src/main.cpp` | 실행 메시지를 STEP13에 맞게 변경 |
| `Makefile` | `UploadHandler.cpp` 빌드 대상 추가 |
| `config/step13.conf` | upload 검증용 config |
| `www/upload.html` | multipart upload form 예시 |
| `www/uploads/.gitkeep` | upload store directory 생성 |

## Upload Route 검사

EventLoop에서 POST 요청 처리 순서는 다음과 같다.

```text
method가 route에서 허용되는지 검사
body size limit 검사
POST /echo는 STEP12 테스트 handler로 처리
그 외 POST는 uploadEnabled 검사
upload off이면 403
upload on이면 UploadHandler 실행
```

upload store는 handler에서 확인한다.

```text
stat(upload_store)
directory인지 확인
W_OK | X_OK 접근 가능한지 확인
```

실패하면 `403 Forbidden`을 반환한다.

## Raw Body Upload

multipart가 아닌 POST body는 raw body로 저장한다.

filename 정책:

```text
upload-<time>-<counter>.bin
```

예:

```text
POST /upload
Content-Type: application/octet-stream
body: raw-body

-> www/uploads/upload-1783557454-1.bin
```

body는 `std::string` 그대로 `write()`로 저장하므로 null byte를 포함해 binary-safe하다.

## Multipart Upload

지원하는 요청:

```http
Content-Type: multipart/form-data; boundary=----abc
```

구현한 내용:

```text
boundary parameter 추출
시작 boundary 검사
part header/body 분리
Content-Disposition header 파싱
filename parameter 추출
boundary 직전 CRLF가 파일 content에 섞이지 않도록 범위 계산
여러 file part 저장
```

저장 성공 response body는 파일마다 한 줄씩 기록한다.

```text
created README.md
updated README.md
```

## Filename 검증

다음 filename은 거절한다.

```text
빈 filename
.
..
slash 포함
backslash 포함
제어 문자 포함
```

따라서 다음 값은 `400 Bad Request`가 된다.

```text
../outside.txt
/tmp/outside.txt
filename=""
```

## 파일 저장

저장 전 `access(path, F_OK)`로 기존 파일 여부를 확인한다.

```text
기존 파일 없음 -> 201 Created
기존 파일 있음 -> 200 OK
```

파일 저장은 다음 방식이다.

```text
open(O_WRONLY | O_CREAT | O_TRUNC, 0644)
write() loop로 partial write 처리
close() 결과 확인
```

## 테스트 config

`config/step13.conf`:

```conf
server {
    listen 127.0.0.1:8080;
    root ./www;
    index index.html;
    client_max_body_size 10M;

    location / {
        methods GET;
    }

    location /upload {
        methods GET POST;
        upload on;
        upload_store ./www/uploads;
    }

    location /post-only {
        methods POST;
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

multipart 새 파일:

```bash
curl -i -F file=@README.md http://127.0.0.1:8080/upload
```

결과:

```text
201 Created
Location: /uploads/README.md
body: created README.md
```

원본 비교:

```bash
cmp README.md www/uploads/README.md
```

결과:

```text
동일
```

multipart overwrite:

```bash
curl -i -F file=@README.md http://127.0.0.1:8080/upload
```

결과:

```text
200 OK
body: updated README.md
```

raw body:

```bash
curl -i -X POST --data-binary raw-body http://127.0.0.1:8080/upload
```

결과:

```text
201 Created
body: created upload-*.bin
```

binary multipart:

```bash
printf 'a\000bcd' > /tmp/step13-binary.bin
curl -i -F file=@/tmp/step13-binary.bin http://127.0.0.1:8080/upload
cmp /tmp/step13-binary.bin www/uploads/step13-binary.bin
```

결과:

```text
201 Created
cmp 동일
```

upload off:

```bash
curl -i -X POST --data-binary hello http://127.0.0.1:8080/post-only
```

결과:

```text
403 Forbidden
```

위험한 filename:

```bash
curl -i -F 'file=@README.md;filename=../outside.txt' http://127.0.0.1:8080/upload
curl -i -F 'file=@README.md;filename=' http://127.0.0.1:8080/upload
```

결과:

```text
400 Bad Request
```

boundary 누락:

```bash
curl -i -X POST -H 'Content-Type: multipart/form-data' --data-binary abc http://127.0.0.1:8080/upload
```

결과:

```text
400 Bad Request
```

## 테스트로 생성된 파일

검증 과정에서 다음 파일이 생성되었다.

```text
www/uploads/README.md
www/uploads/step13-binary.bin
www/uploads/upload-*.bin
```

## 이번 단계의 한계

- multipart form field 중 파일이 아닌 일반 field는 저장하지 않고 무시한다.
- multipart nested 구조는 지원하지 않는다.
- raw body filename은 서버가 생성한다.
- 업로드 성공 response는 간단한 text/plain 목록이다.

## 완료 체크

- [x] upload 설정과 method를 검사한다.
- [x] raw body upload가 동작한다.
- [x] multipart boundary와 part를 파싱한다.
- [x] filename을 안전하게 검증한다.
- [x] text 파일이 원본과 동일하게 저장된다.
- [x] binary 파일이 원본과 동일하게 저장된다.
- [x] 신규 생성과 overwrite status를 구분한다.
- [x] root/upload store 밖 파일 생성을 막는다.

다음 단계는 `STEP14.md`의 DELETE handler 구현이다.
