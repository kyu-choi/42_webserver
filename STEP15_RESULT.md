# STEP 15 Result - Transfer-Encoding: chunked

## 구현 요약

이번 단계에서는 `Transfer-Encoding: chunked` request body를 파싱해서 chunk framing을 제거하고, handler가 항상 decoded body만 보도록 연결했다.

예를 들어 다음 요청은:

```http
POST /echo HTTP/1.1
Host: localhost
Transfer-Encoding: chunked

5\r
hello\r
1\r
 \r
5\r
world\r
0\r
\r
```

handler에는 raw body인 `5\r\nhello...`가 아니라 `hello world`만 전달된다.

## 추가 및 수정된 파일

| 파일 | 내용 |
| --- | --- |
| `include/webserv/StateMachine.hpp` | chunked trailer를 읽는 `PARSER_BODY_CHUNK_TRAILERS` 상태 추가 |
| `src/core/StateMachine.cpp` | 새 parser 상태 이름을 `"BODY_CHUNK_TRAILERS"`로 출력 |
| `include/webserv/RequestParser.hpp` | chunked 여부를 header parser에서 돌려주고, chunked body parser 함수 선언 추가 |
| `src/http/RequestParser.cpp` | `Transfer-Encoding: chunked` 판별, hex chunk size 파싱, chunk data decode, trailer 소비 구현 |
| `src/core/EventLoop.cpp` | chunked parser 상태를 body 수신 상태로 취급하고, body limit을 decoded body 기준으로 검사 |
| `src/main.cpp` | 실행 로그를 STEP15 기준으로 갱신 |
| `config/step15.conf` | STEP15 테스트용 `/echo`, `/upload` location 추가 |

## RequestParser 변경

### 1. chunk size line 파싱

`src/http/RequestParser.cpp`에 다음 보조 함수가 추가되었다.

```cpp
bool isHexDigit(char value);
int hexDigitValue(char value);
bool parseChunkSizeLine(const std::string& line, std::size_t& parsed);
```

`parseChunkSizeLine()`은 다음 정책을 가진다.

- chunk size는 16진수로 파싱한다.
- `5;name=value` 같은 chunk extension은 `;` 뒤를 버리고 size만 사용한다.
- 빈 size, 16진수가 아닌 문자, `std::size_t` overflow는 실패로 처리한다.
- 실패하면 parser error가 되고 서버는 `400 Bad Request`를 응답한다.

### 2. Transfer-Encoding header 처리

`parseHeaders()`가 `Transfer-Encoding`을 case-insensitive하게 확인한다.

구현 정책:

- `Transfer-Encoding: chunked`만 지원한다.
- 다른 transfer encoding은 `501 Not Implemented`로 처리한다.
- `Content-Length`와 `Transfer-Encoding`이 동시에 있으면 `400 Bad Request`로 거절한다.
- `Transfer-Encoding` header가 중복되어도 `400 Bad Request`로 거절한다.

chunked 요청이면:

```cpp
request.setBodyFraming(HttpRequest::BODY_CHUNKED);
hasChunkedBody = true;
```

를 설정해서 이후 body parser가 chunked path로 들어간다.

### 3. chunked body decode

새 함수 `parseChunkedBody()`가 추가되었다.

동작 순서:

1. `PARSER_BODY_CHUNK_SIZE`
   다음 `\r\n`까지 size line을 기다린다.

2. size line 파싱
   size line을 16진수 숫자로 변환한다.

3. `PARSER_BODY_CHUNK_DATA`
   size만큼 data가 올 때까지 기다린다.

4. decoded body append
   data 부분만 `decodedBody`에 append한다.

5. `PARSER_BODY_CHUNK_CRLF`
   chunk data 뒤에 정확히 `\r\n`이 있는지 검사한다.

6. size가 `0`이면 `PARSER_BODY_CHUNK_TRAILERS`
   trailer header가 있으면 마지막 `\r\n\r\n`까지 소비한다.

7. 완료되면
   ```cpp
   request.setBody(decodedBody);
   _state = PARSER_COMPLETE;
   ```

이 구조 덕분에 `/echo`, `/upload`, 이후 CGI 실행기는 모두 `request.body()`를 통해 decoded body를 받는다.

현재 코드베이스에는 CGI 실행기가 아직 없어서 실제 CGI POST 실행 테스트는 하지 않았다. 다만 STEP15에서 만든 request body 경로는 CGI가 추가될 때 그대로 stdin 입력으로 사용할 수 있는 형태다.

## EventLoop 변경

### 1. chunked parser 상태를 body 수신 상태로 취급

기존에는 incomplete 상태일 때 `PARSER_BODY_BY_LENGTH`만 body 수신 중으로 봤다.

이번 단계에서 `parserStateReadsBody()`를 추가해서 다음 상태들도 `CLIENT_READING_BODY`로 처리한다.

```cpp
PARSER_BODY_BY_LENGTH
PARSER_BODY_CHUNK_SIZE
PARSER_BODY_CHUNK_DATA
PARSER_BODY_CHUNK_CRLF
PARSER_BODY_CHUNK_TRAILERS
```

그래서 chunked body가 여러 번 나뉘어 도착해도 서버는 계속 읽기 이벤트를 감시한다.

### 2. client_max_body_size 기준 수정

STEP15에서 중요한 점은 body limit을 raw input buffer 크기가 아니라 decoded body 크기에 적용해야 한다는 것이다.

그래서 기존 raw body 수신량 계산 함수는 제거하고, early body limit 검사도 `bodyTooLarge()`를 사용하도록 바꿨다.

현재 기준:

```cpp
if (request.hasContentLength()
    && request.contentLength() > clientMaxBodySize)
    return (true);
return (request.body().size() > clientMaxBodySize);
```

의미:

- `Content-Length` 요청은 header에 선언된 길이가 limit을 넘으면 조기 413 처리한다.
- chunked 요청은 지금까지 decode된 `request.body().size()`가 limit을 넘으면 413 처리한다.
- chunk framing byte 때문에 정상 body가 413이 되는 문제를 피한다.

## 테스트용 설정

`config/step15.conf`를 추가했다.

중요한 location:

```conf
location /echo {
    methods POST;
    client_max_body_size 11;
}

location /upload {
    methods POST;
    upload on;
    upload_store ./www/uploads;
    client_max_body_size 32;
}
```

`/echo`는 `hello world` 11바이트 경계 테스트용이고, `/upload`는 chunked upload가 decoded body를 저장하는지 확인하기 위한 location이다.

## 실행한 검증

빌드:

```sh
make
```

결과:

```text
webserv 빌드 성공
```

서버 실행:

```sh
./webserv config/step15.conf
```

### 정상 요청

| 테스트 | 기대 | 결과 |
| --- | --- | --- |
| 한 개 chunk `5\r\nhello\r\n0\r\n\r\n` | `200 OK`, body `hello` | 통과 |
| 여러 chunk `hello` + space + `world` | `200 OK`, body `hello world` | 통과 |
| 빈 body `0\r\n\r\n` | `200 OK`, `Content-Length: 0` | 통과 |
| chunk extension `5;name=value` | extension 무시, body `hello` | 통과 |
| trailer header | trailer 소비 후 body `hello` | 통과 |
| `/upload` chunked raw upload | decoded body만 저장 | 통과 |

업로드 검증:

```text
www/uploads/upload-1783559033-1.bin: chunked-upload-step15
```

저장 파일 hex 확인 결과도 `chunked-upload-step15`만 있었고, `f\r\n`, `6\r\n`, `0\r\n` 같은 chunk framing byte는 섞이지 않았다.

### 오류 요청

| 테스트 | 기대 | 결과 |
| --- | --- | --- |
| 잘못된 hex size `Z` | `400 Bad Request` | 통과 |
| chunk data 뒤 CRLF 누락 | `400 Bad Request` | 통과 |
| `Content-Length`와 chunked 동시 사용 | `400 Bad Request` | 통과 |
| unsupported `Transfer-Encoding: gzip` | `501 Not Implemented` | 통과 |
| decoded body 12바이트, limit 11바이트 | `413 Payload Too Large` | 통과 |
| data 길이가 부족한 요청 | 성공 응답 없이 연결 종료 | 통과 |

## 현재 한계

- trailer header는 안전하게 소비하지만 별도 header로 저장하지 않는다.
- `Transfer-Encoding`은 `chunked` 하나만 지원한다.
- CGI 실행기는 아직 코드베이스에 없어서 실제 CGI POST 실행 테스트는 이후 CGI 단계에서 진행해야 한다.
- keep-alive/pipelining은 아직 구현 범위가 아니며, 응답 후 연결을 닫는다.

## 완료 기준 체크

- [x] chunk size를 16진수로 파싱한다.
- [x] 여러 chunk data를 정확히 합친다.
- [x] 종료 chunk와 trailer 끝을 처리한다.
- [x] malformed chunk를 `400`으로 처리한다.
- [x] decoded body 기준으로 `413`을 판단한다.
- [x] upload handler가 decoded body를 받는다.
- [x] CGI가 사용할 request body 경로를 decoded body로 만든다.
- [x] malformed 요청 후 서버가 계속 동작한다.

다음 단계는 `STEP16.md`다.
