#!/usr/bin/env python3
import sys
import threading
import time

from http_utils import (
    Response,
    Server,
    assert_contains,
    assert_status,
    main_with_cases,
    raw_request,
    request,
)


def cgi_request_features():
    with Server("config/step18.conf", ports=(8080, 8081)):
        response = request("GET", "/cgi-bin/hello.py?name=test")
        assert_status(response, 200, "cgi get")
        assert_contains(response, "method=GET", "cgi get method")
        assert_contains(response, "query=name=test", "cgi get query")

        response = request(
            "POST",
            "/cgi-bin/hello.py",
            headers={"Content-Type": "application/x-www-form-urlencoded"},
            body=b"name=test",
        )
        assert_status(response, 200, "cgi post")
        assert_contains(response, "content_length=9", "cgi post length")
        assert_contains(response, "body=name=test", "cgi post body")

        chunked = (
            b"POST /cgi-bin/hello.py HTTP/1.1\r\n"
            b"Host: localhost\r\n"
            b"Content-Type: text/plain\r\n"
            b"Transfer-Encoding: chunked\r\n"
            b"\r\n"
            b"5\r\nhello\r\n1\r\n \r\n5\r\nworld\r\n0\r\n\r\n"
        )
        response = Response(raw_request(chunked, timeout=5))
        assert_status(response, 200, "cgi chunked")
        assert_contains(response, "body=hello world", "cgi chunked decoded body")

        bad = request("GET", "/cgi-bin/bad_output.py")
        assert_status(bad, 502, "cgi bad output")

        missing = request("GET", "/cgi-bin/missing.py")
        assert_status(missing, 404, "cgi missing script")


def cgi_timeout_does_not_block_static_request():
    with Server("config/step18.conf", ports=(8080, 8081)):
        result = {}

        def timeout_request():
            result["timeout"] = request("GET", "/cgi-bin/timeout.py", timeout=6)

        thread = threading.Thread(target=timeout_request)
        thread.start()
        time.sleep(0.2)
        started = time.time()
        static_response = request("GET", "/", timeout=2)
        elapsed = time.time() - started
        assert_status(static_response, 200, "static during cgi timeout")
        if elapsed > 1.0:
            raise AssertionError("static request waited %.2fs during CGI" % elapsed)
        thread.join(8)
        if "timeout" not in result:
            raise AssertionError("timeout CGI request did not finish")
        assert_status(result["timeout"], 504, "cgi timeout")


if __name__ == "__main__":
    sys.exit(main_with_cases([
        ("cgi request features", cgi_request_features),
        ("cgi timeout is non-blocking", cgi_timeout_does_not_block_static_request),
    ]))
