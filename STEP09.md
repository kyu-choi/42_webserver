# STEP 09 - 여러 포트와 Location Routing 구현

## 이 단계의 목표

하나의 `webserv` 프로세스가 여러 interface:port에서 요청을 받고, URI에 맞는 location과 실제 파일 경로를 선택하게 한다.

## 선행 조건

- [ ] [STEP08.md](STEP08.md)의 config parser가 완료되었다.
- [ ] 여러 ServerConfig와 LocationConfig가 내부 구조에 저장된다.
- [ ] [STEP07.md](STEP07.md)의 정적 파일 handler가 동작한다.

## 만들 파일 예시

```text
include/Router.hpp
src/http/Router.cpp
www/site_b/
```

## 사용자가 해야 할 일

### 1. config 기반 listen socket 생성

- [ ] 모든 ServerConfig의 interface:port 쌍을 순회한다.
- [ ] 각 쌍에 listen socket을 생성한다.
- [ ] 각 listen fd를 non-blocking으로 설정한다.
- [ ] 모든 listen fd를 같은 poll loop에 등록한다.
- [ ] listen fd와 ServerConfig의 관계를 저장한다.

필요한 관계:

```text
listen fd → 어떤 server config를 사용하는가
client fd → 어느 listen fd에서 accept되었는가
```

### 2. client에 server 정보 연결

- [ ] accept 시 client에 listen fd를 저장한다.
- [ ] request 처리 시 listen fd로 ServerConfig를 찾는다.
- [ ] port별로 다른 root와 콘텐츠를 제공한다.

Virtual host는 Mandatory 범위 밖이므로 같은 port의 Host 기반 server 선택은 Mandatory 완료 전에는 추가하지 않아도 된다.

### 3. longest-prefix location 선택

예:

```text
요청: /images/icons/home.png

location /
location /images
location /images/icons

선택: /images/icons
```

- [ ] request path의 prefix와 일치하는 location을 찾는다.
- [ ] 일치한 location 중 prefix 길이가 가장 긴 것을 선택한다.
- [ ] 아무 location도 없을 때 server 기본 설정을 사용한다.
- [ ] `/image`가 `/images`에 잘못 매칭되지 않도록 segment 경계를 검토한다.

### 4. effective config 계산

- [ ] location에서 명시한 값은 location 값을 사용한다.
- [ ] location에서 명시하지 않은 값은 server 값을 사용한다.
- [ ] root, index, body size 등 상속 대상과 비대상을 정한다.
- [ ] 요청 처리 중 매번 일관된 effective config를 얻는다.

### 5. URI를 실제 path로 변환

정해둔 규칙에 따라:

```text
요청 URI: /images/icons/home.png
선택 location: /images
location root: ./www/images
실제 path: ./www/images/icons/home.png
```

- [ ] query를 제거한다.
- [ ] location prefix를 올바르게 제거한다.
- [ ] root와 나머지 path를 결합한다.
- [ ] path를 정규화하고 traversal을 검사한다.
- [ ] root 밖 결과를 거절한다.

### 6. Router 결과 구조 작성

Router가 handler에 제공할 정보:

```text
선택된 ServerConfig
선택된 LocationConfig 또는 effective config
정규화된 URI path
query string
실제 filesystem path
처리해야 할 handler 종류
routing error status
```

Router는 socket I/O를 직접 하지 않도록 한다.

## 테스트 config 예시

```conf
server {
    listen 127.0.0.1:8080;
    root ./www;

    location /images {
        root ./www/images;
    }

    location /images/icons {
        root ./www/icons;
    }
}

server {
    listen 127.0.0.1:8081;
    root ./www/site_b;
}
```

## 실행 및 검증

```bash
curl -i http://127.0.0.1:8080/
curl -i http://127.0.0.1:8081/
curl -i http://127.0.0.1:8080/images/a.png
curl -i http://127.0.0.1:8080/images/icons/a.png
```

경계 테스트:

```text
/images
/images/
/images/icons
/images-other
/../../etc/passwd
```

## 자주 발생하는 문제

### 모든 port가 같은 콘텐츠를 제공함

확인:

- client가 들어온 listen fd를 저장하는가?
- fd별 ServerConfig 관계가 유지되는가?

### 짧은 location이 선택됨

확인:

- 첫 일치에서 종료하지 않는가?
- 가장 긴 prefix를 비교하는가?

### root가 중복되어 path가 이상함

확인:

- location prefix 제거 규칙이 일관적인가?
- slash 결합 helper를 사용하는가?

## 완료 조건

- [ ] 여러 listen socket이 같은 poll loop에서 동작한다.
- [ ] 서로 다른 port가 서로 다른 콘텐츠를 제공한다.
- [ ] longest-prefix location을 선택한다.
- [ ] server 값과 location override를 정확히 합친다.
- [ ] Router가 안전한 실제 path를 제공한다.
- [ ] traversal로 root 밖에 접근할 수 없다.

다음 단계: [STEP10.md](STEP10.md)
