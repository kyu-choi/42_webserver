#!/usr/bin/env python3
import sys

from http_utils import (
    Response,
    Server,
    assert_status,
    main_with_cases,
    raw_request,
    request,
)


def malformed_requests_do_not_kill_server():
    with Server("config/step18.conf", ports=(8080, 8081)):
        response = Response(raw_request(b"BAD\r\n\r\n"))
        assert_status(response, 400, "bad request line")

        response = Response(raw_request(b"GET / HTTP/1.1\r\n\r\n"))
        assert_status(response, 400, "missing host")

        response = Response(raw_request(
            b"GET / HTTP/1.1\r\nHost localhost\r\n\r\n"
        ))
        assert_status(response, 400, "bad header")

        response = Response(raw_request(
            b"POST /echo HTTP/1.1\r\n"
            b"Host: localhost\r\n"
            b"Content-Length: nope\r\n"
            b"\r\n"
        ))
        assert_status(response, 400, "bad content-length")

        alive = request("GET", "/")
        assert_status(alive, 200, "server alive after malformed requests")


if __name__ == "__main__":
    sys.exit(main_with_cases([
        ("malformed requests", malformed_requests_do_not_kill_server),
    ]))
