# STEP 11 - Directory Index와 Autoindex 구현

## 이 단계의 목표

요청한 실제 path가 directory일 때 설정에 따라 index 파일을 제공하거나 directory listing HTML을 생성한다.

## 선행 조건

- [ ] [STEP10.md](STEP10.md)가 완료되었다.
- [ ] Router가 실제 filesystem path와 effective config를 제공한다.
- [ ] 정적 파일 response와 error response가 동작한다.

## 처리 우선순위

```text
directory 확인
→ 설정된 index 파일을 순서대로 확인
→ index가 있으면 정적 파일 response
→ index가 없고 autoindex on이면 listing 생성
→ index가 없고 autoindex off이면 403
```

## 사용자가 해야 할 일

### 1. directory 판별

- [ ] `stat()` 결과로 regular file과 directory를 구분한다.
- [ ] 존재하지 않는 path는 `404`로 처리한다.
- [ ] 접근할 수 없는 directory는 `403`으로 처리한다.
- [ ] regular file 처리와 directory 처리를 분리한다.

### 2. index 파일 검색

설정 예:

```conf
index index.html index.htm home.html;
```

- [ ] effective config에서 index 목록을 얻는다.
- [ ] 설정 순서대로 각 index 파일 path를 만든다.
- [ ] 존재하는 첫 번째 regular file을 제공한다.
- [ ] index가 directory이거나 접근 불가인 경우의 정책을 정한다.
- [ ] index URI와 실제 path가 올바르게 연결되는지 검사한다.

### 3. autoindex 여부 판단

- [ ] index를 찾지 못한 뒤에만 autoindex를 확인한다.
- [ ] `autoindex off`이면 `403 Forbidden`을 반환한다.
- [ ] `autoindex on`이면 directory listing을 생성한다.

### 4. directory listing 생성

- [ ] `opendir()`로 directory를 연다.
- [ ] `readdir()`로 entry를 순회한다.
- [ ] `closedir()`를 모든 경로에서 호출한다.
- [ ] 각 entry를 HTML link로 만든다.
- [ ] directory entry에는 `/` suffix를 표시한다.
- [ ] 현재 요청 URI 기준으로 link를 만든다.
- [ ] filename을 HTML escape한다.
- [ ] 정렬 여부를 결정하고 일관되게 적용한다.

예시 결과:

```html
<!doctype html>
<html>
<body>
  <h1>Index of /uploads/</h1>
  <a href="/uploads/a.txt">a.txt</a>
  <a href="/uploads/images/">images/</a>
</body>
</html>
```

### 5. URI slash 정책

다음 요청의 정책을 정한다.

```text
/uploads
/uploads/
```

권장:

- directory URI가 slash 없이 들어오면 slash가 붙은 URI로 redirect하거나, link 생성이 깨지지 않도록 정규화한다.

### 6. 보안 검사

- [ ] `.`와 `..` entry 표시 정책을 정한다.
- [ ] listing에서 root 밖 link를 만들지 않는다.
- [ ] 숨김 파일 표시 정책을 정한다.
- [ ] filename의 `<`, `>`, `&`, `"`, `'`를 escape한다.
- [ ] directory 내부 symlink 처리 정책을 검토한다.

## 테스트용 구조

```text
www/
├── index.html
├── listing/
│   ├── a.txt
│   ├── b.txt
│   └── images/
└── private-directory/
```

config:

```conf
location /listing {
    autoindex on;
}

location /private-directory {
    autoindex off;
}
```

## 실행 및 검증

```bash
curl -i http://127.0.0.1:8080/
curl -i http://127.0.0.1:8080/listing/
curl -i http://127.0.0.1:8080/private-directory/
```

브라우저에서:

- listing의 각 link 클릭.
- 하위 directory 이동.
- index가 있는 directory가 listing 대신 index를 제공하는지 확인.

## 자주 발생하는 문제

### autoindex on인데 index가 있어도 listing이 나옴

해결:

- index 검색을 autoindex보다 먼저 수행한다.

### link 클릭 시 잘못된 URI로 이동

확인:

- base URI 끝 slash를 처리하는가?
- filename을 URI에 안전하게 결합하는가?

### HTML injection 가능

해결:

- filesystem filename을 그대로 HTML에 넣지 말고 escape한다.

## 완료 조건

- [ ] directory와 regular file을 구분한다.
- [ ] index 파일을 설정 순서대로 찾는다.
- [ ] index가 autoindex보다 우선한다.
- [ ] autoindex on에서 안전한 listing HTML을 만든다.
- [ ] autoindex off에서 `403`을 반환한다.
- [ ] listing link가 브라우저에서 정상 동작한다.

다음 단계: [STEP12.md](STEP12.md)
