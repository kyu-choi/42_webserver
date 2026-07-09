#!/usr/bin/env python3
import sys

from http_utils import Response, Server, assert_contains, assert_status, main_with_cases, raw_request


def chunked_request(path, chunk_block, extra_headers=b""):
    raw = (
        b"POST " + path.encode("ascii") + b" HTTP/1.1\r\n"
        b"Host: localhost\r\n"
        + extra_headers +
        b"Transfer-Encoding: chunked\r\n"
        b"\r\n"
        + chunk_block
    )
    return Response(raw_request(raw))


def chunked_decoding_and_errors():
    with Server("config/step18.conf", ports=(8080, 8081)):
        response = chunked_request(
            "/echo",
            b"5\r\nhello\r\n1\r\n \r\n5\r\nworld\r\n0\r\n\r\n",
        )
        assert_status(response, 200, "multi chunk")
        if response.body != b"hello world":
            raise AssertionError("decoded body mismatch: %r" % response.body)

        response = chunked_request(
            "/echo",
            b"1\r\nh\r\n1\r\ne\r\n1\r\nl\r\n1\r\nl\r\n1\r\no\r\n0\r\n\r\n",
        )
        assert_status(response, 200, "many small chunks")
        if response.body != b"hello":
            raise AssertionError("small chunk decoded body mismatch")

        response = chunked_request("/echo", b"b\r\nhello world\r\n0\r\n\r\n")
        assert_status(response, 200, "decoded limit boundary")

        response = chunked_request("/echo", b"c\r\nhello world!\r\n0\r\n\r\n")
        assert_status(response, 413, "decoded limit plus one")

        response = chunked_request("/echo", b"Z\r\nhello\r\n0\r\n\r\n")
        assert_status(response, 400, "bad hex chunk size")

        response = chunked_request("/echo", b"5\r\nhello0\r\n\r\n")
        assert_status(response, 400, "missing chunk crlf")

        raw = (
            b"POST /echo HTTP/1.1\r\n"
            b"Host: localhost\r\n"
            b"Content-Length: 5\r\n"
            b"Transfer-Encoding: chunked\r\n"
            b"\r\nhello"
        )
        response = Response(raw_request(raw))
        assert_status(response, 400, "content-length plus chunked")

        response = chunked_request(
            "/cgi-bin/hello.py",
            b"5\r\nhello\r\n1\r\n \r\n5\r\nworld\r\n0\r\n\r\n",
            extra_headers=b"Content-Type: text/plain\r\n",
        )
        assert_status(response, 200, "chunked cgi")
        assert_contains(response, "content_length=11", "chunked cgi length")
        assert_contains(response, "body=hello world", "chunked cgi body")


if __name__ == "__main__":
    sys.exit(main_with_cases([
        ("chunked requests", chunked_decoding_and_errors),
    ]))
