# STEP 11 구현 결과

## 목표

`STEP11.md`의 목표는 요청한 실제 path가 directory일 때 설정에 따라 index 파일을 제공하거나 autoindex listing HTML을 생성하는 것이다.

이번 단계에서 directory 처리 순서를 다음처럼 구현했다.

```text
stat()으로 path 확인
-> regular file이면 파일 응답
-> directory이면 slash 없는 URI는 slash 있는 URI로 redirect
-> index 목록을 설정 순서대로 검색
-> index regular file이 있으면 파일 응답
-> index가 없고 autoindex on이면 directory listing HTML 생성
-> index가 없고 autoindex off이면 403
```

## 생성/수정한 파일

| 경로 | 구현 내용 |
|---|---|
| `include/webserv/StaticFileHandler.hpp` | `handlePath()`에 `autoindex`, `uriPath` 인자 추가 |
| `src/handlers/StaticFileHandler.cpp` | directory 판별, slash redirect, index 검색, autoindex listing 생성 |
| `src/core/EventLoop.cpp` | Router의 effective autoindex와 URI path를 static handler에 전달 |
| `src/http/Router.cpp` | trailing slash 보존, location root 상속 시 filesystem path 계산 수정 |
| `src/main.cpp` | 실행 메시지를 STEP11에 맞게 변경 |
| `config/step11.conf` | directory index/autoindex 검증용 config |
| `www/listing/*` | autoindex listing 검증용 파일과 하위 directory |
| `www/private-directory/.gitkeep` | autoindex off 검증용 directory |
| `www/has-index/home.html` | autoindex보다 index가 우선되는지 검증 |

## Directory 판별

`StaticFileHandler::handlePath()`에서 `stat()`으로 path 상태를 확인한다.

```text
없음              -> 404
regular file      -> 파일 응답
directory         -> directory 처리
그 외 타입         -> 403
```

directory는 `buildDirectoryResponse()`에서 따로 처리한다.

## Index 검색

effective config의 `index` 목록을 순서대로 확인한다.

```conf
index home.html index.html;
```

예:

```text
/has-index/
-> ./www/has-index/home.html 검사
-> 있으면 home.html 응답
-> autoindex on이어도 listing 생성하지 않음
```

index 후보가 directory이면 `403`으로 처리한다. readable regular file이면 기존 파일 응답 로직을 사용한다.

## Autoindex 구현

index 파일을 찾지 못했고 `autoindex on`이면 `opendir()`/`readdir()`로 directory entry를 읽어 HTML listing을 만든다.

구현한 정책:

```text
정렬: 이름 기준 오름차순
숨김 파일: 표시하지 않음
. / ..: 표시하지 않음
directory entry: 이름 뒤에 / 표시
HTML escape: &, <, >, ", ' 처리
URI encode: link href에 안전하지 않은 문자를 percent-encode
```

예시:

```html
<h1>Index of /listing/</h1>
<ul>
<li><a href="/listing/a.txt">a.txt</a></li>
<li><a href="/listing/b.txt">b.txt</a></li>
<li><a href="/listing/images/">images/</a></li>
</ul>
```

## Slash 정책

directory URI가 slash 없이 들어오면 slash가 붙은 URI로 redirect한다.

```text
/listing
-> 301 Location: /listing/

/has-index
-> 301 Location: /has-index/
```

이렇게 하면 directory listing 안의 상대 link가 브라우저에서 깨지지 않는다.

## Router 수정

테스트 중 두 가지 routing 문제를 고쳤다.

첫 번째는 trailing slash 보존이다.

기존 `PathPolicy` 정규화는 `/listing/`을 `/listing`으로 줄였다. 그래서 이미 slash가 붙은 request도 다시 `/listing/`으로 redirect하는 loop가 생겼다.

수정 후에는 원본 request path가 slash로 끝났다면 `RouteResult.uriPath`에도 slash를 보존한다.

두 번째는 location root 상속이다.

location이 `root`를 따로 설정하지 않았는데도 location prefix를 제거하면 `/listing/`이 `./www/listing`이 아니라 `./www`로 매핑되는 문제가 있었다.

수정 후:

```text
location root override 있음
-> location prefix 제거

location root override 없음
-> server root 기준으로 전체 URI path 사용
```

## 테스트 config

`config/step11.conf`:

```conf
server {
    listen 127.0.0.1:8080;
    root ./www;
    index index.html;

    location /listing {
        methods GET;
        autoindex on;
    }

    location /private-directory {
        methods GET;
        autoindex off;
    }

    location /has-index {
        methods GET;
        index home.html index.html;
        autoindex on;
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

서버 실행:

```bash
./webserv config/step11.conf
```

확인한 출력:

```text
webserv STEP11 directory index server
listen sockets: 1
```

HTTP 요청 검증:

```bash
curl -i http://127.0.0.1:8080/
curl -i http://127.0.0.1:8080/listing
curl -i http://127.0.0.1:8080/listing/
curl -i http://127.0.0.1:8080/listing/a.txt
curl -i http://127.0.0.1:8080/listing/images/
curl -i http://127.0.0.1:8080/listing/images/item.txt
curl -i http://127.0.0.1:8080/private-directory/
curl -i http://127.0.0.1:8080/has-index
curl -i http://127.0.0.1:8080/has-index/
```

확인한 결과:

```text
/                              -> 200 OK, ./www/index.html
/listing                       -> 301 Location: /listing/
/listing/                      -> 200 OK, autoindex listing
/listing/a.txt                 -> 200 OK, alpha
/listing/images/               -> 200 OK, 하위 autoindex listing
/listing/images/item.txt       -> 200 OK
/private-directory/            -> 403 Forbidden
/has-index                     -> 301 Location: /has-index/
/has-index/                    -> 200 OK, home.html
```

## 이번 단계의 한계

- 숨김 파일은 listing에서 표시하지 않는다.
- `.`와 `..`도 listing에서 표시하지 않는다.
- directory symlink는 `stat()` 결과에 따라 directory처럼 보일 수 있다.
- autoindex HTML은 간단한 목록 형태이며 CSS styling은 없다.

## 완료 체크

- [x] directory와 regular file을 구분한다.
- [x] index 파일을 설정 순서대로 찾는다.
- [x] index가 autoindex보다 우선한다.
- [x] autoindex on에서 안전한 listing HTML을 만든다.
- [x] autoindex off에서 `403`을 반환한다.
- [x] slash 없는 directory URI를 slash 있는 URI로 redirect한다.
- [x] listing link가 브라우저에서 정상 동작할 수 있는 href를 만든다.

다음 단계는 `STEP12.md`의 file upload POST handler 구현이다.
