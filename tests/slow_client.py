#!/usr/bin/env python3
import socket
import sys
import time

from http_utils import Response, Server, assert_status, main_with_cases, raw_request, request


def send_one_byte_at_a_time(raw, delay=0.01):
    with socket.create_connection(("127.0.0.1", 8080), timeout=4) as sock:
        sock.settimeout(4)
        for byte in raw:
            sock.sendall(bytes([byte]))
            time.sleep(delay)
        data = b""
        while True:
            chunk = sock.recv(65536)
            if not chunk:
                break
            data += chunk
        return Response(data)


def slow_and_disconnect_clients():
    with Server("config/step18.conf", ports=(8080, 8081)):
        response = send_one_byte_at_a_time(
            b"GET / HTTP/1.1\r\nHost: localhost\r\n\r\n"
        )
        assert_status(response, 200, "slow header")

        response = send_one_byte_at_a_time(
            b"POST /echo HTTP/1.1\r\n"
            b"Host: localhost\r\n"
            b"Content-Length: 5\r\n"
            b"\r\nhello"
        )
        assert_status(response, 200, "slow body")
        if response.body != b"hello":
            raise AssertionError("slow body echo mismatch")

        raw_request(
            b"POST /echo HTTP/1.1\r\n"
            b"Host: localhost\r\n"
            b"Content-Length: 10\r\n"
            b"\r\nabc",
            read_response=False,
        )

        with socket.create_connection(("127.0.0.1", 8080), timeout=4) as sock:
            sock.sendall(b"GET / HTTP/1.1\r\nHost: localhost\r\n\r\n")
            time.sleep(0.1)

        alive = request("GET", "/")
        assert_status(alive, 200, "server alive after slow/disconnect clients")


if __name__ == "__main__":
    sys.exit(main_with_cases([
        ("slow and disconnect clients", slow_and_disconnect_clients),
    ]))
