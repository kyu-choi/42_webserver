#!/usr/bin/env python3
import os
import signal
import socket
import subprocess
import sys
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
WEBSERV = ROOT / "webserv"


class TestFailure(Exception):
    pass


class Response:
    def __init__(self, raw):
        self.raw = raw
        self.status = 0
        self.reason = ""
        self.headers = {}
        self.body = b""
        self._parse(raw)

    def _parse(self, raw):
        marker = raw.find(b"\r\n\r\n")
        if marker < 0:
            raise TestFailure("response missing header terminator: %r" % raw[:120])
        head = raw[:marker].decode("iso-8859-1")
        self.body = raw[marker + 4:]
        lines = head.split("\r\n")
        parts = lines[0].split(" ", 2)
        if len(parts) < 2 or not parts[1].isdigit():
            raise TestFailure("bad status line: %r" % lines[0])
        self.status = int(parts[1])
        self.reason = parts[2] if len(parts) > 2 else ""
        for line in lines[1:]:
            if ":" not in line:
                raise TestFailure("bad header line: %r" % line)
            name, value = line.split(":", 1)
            self.headers[name.strip().lower()] = value.strip()

    def text(self):
        return self.body.decode("utf-8", "replace")


def note(message):
    print("[INFO] " + message)


def pass_(message):
    print("[PASS] " + message)


def fail(message):
    print("[FAIL] " + message)


def raw_request(raw, port=8080, timeout=4, read_response=True):
    with socket.create_connection(("127.0.0.1", port), timeout=timeout) as sock:
        sock.settimeout(timeout)
        sock.sendall(raw)
        if not read_response:
            return b""
        data = b""
        while True:
            try:
                chunk = sock.recv(65536)
            except socket.timeout:
                break
            if not chunk:
                break
            data += chunk
        return data


def request(method, path, port=8080, headers=None, body=b"", timeout=4):
    if headers is None:
        headers = {}
    if isinstance(body, str):
        body = body.encode("utf-8")
    lines = [
        "%s %s HTTP/1.1" % (method, path),
        "Host: 127.0.0.1:%d" % port,
    ]
    has_content_length = False
    for name, value in headers.items():
        if name.lower() == "content-length":
            has_content_length = True
        lines.append("%s: %s" % (name, value))
    if body and not has_content_length:
        lines.append("Content-Length: %d" % len(body))
    raw = ("\r\n".join(lines) + "\r\n\r\n").encode("iso-8859-1") + body
    return Response(raw_request(raw, port=port, timeout=timeout))


def assert_status(response, expected, label):
    if response.status != expected:
        raise TestFailure("%s: expected %d, got %d\n%s" % (
            label,
            expected,
            response.status,
            response.raw.decode("iso-8859-1", "replace")[:500],
        ))


def assert_contains(response, needle, label):
    if isinstance(needle, str):
        needle = needle.encode("utf-8")
    if needle not in response.body:
        raise TestFailure("%s: body does not contain %r\n%s" % (
            label,
            needle,
            response.text()[:500],
        ))


def assert_header(response, name, expected, label):
    value = response.headers.get(name.lower())
    if value != expected:
        raise TestFailure("%s: expected header %s=%r, got %r" % (
            label,
            name,
            expected,
            value,
        ))


def wait_for_port(port, process, timeout=5):
    deadline = time.time() + timeout
    while time.time() < deadline:
        if process.poll() is not None:
            output = read_process_output(process)
            raise TestFailure("server exited before port %d opened\n%s" % (
                port,
                output,
            ))
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=0.2):
                return
        except OSError:
            time.sleep(0.05)
    raise TestFailure("server did not open port %d" % port)


def start_server(config, ports=(8080,), command=None):
    if command is None:
        command = [str(WEBSERV), str(ROOT / config)]
    process = subprocess.Popen(
        command,
        cwd=str(ROOT),
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    try:
        for port in ports:
            wait_for_port(port, process)
    except Exception:
        stop_server(process)
        raise
    return process


def read_process_output(process):
    if process.stdout is None:
        return ""
    try:
        return process.stdout.read()
    except Exception:
        return ""


def stop_server(process):
    if process.poll() is None:
        process.send_signal(signal.SIGINT)
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait(timeout=5)
    return read_process_output(process)


class Server:
    def __init__(self, config, ports=(8080,), command=None):
        self.config = config
        self.ports = ports
        self.command = command
        self.process = None
        self.output = ""

    def __enter__(self):
        self.process = start_server(self.config, self.ports, self.command)
        return self

    def __exit__(self, exc_type, exc, tb):
        self.output = stop_server(self.process)
        return False


def run_case(name, func):
    try:
        func()
        pass_(name)
        return True
    except Exception as error:
        fail("%s: %s" % (name, error))
        return False


def cleanup_uploads(prefix="step18-"):
    upload_dir = ROOT / "www" / "uploads"
    for path in upload_dir.iterdir():
        if path.name.startswith(prefix):
            path.unlink()


def multipart_body(boundary, filename, content, field="file"):
    if isinstance(content, str):
        content = content.encode("utf-8")
    head = (
        "--%s\r\n"
        "Content-Disposition: form-data; name=\"%s\"; filename=\"%s\"\r\n"
        "Content-Type: application/octet-stream\r\n"
        "\r\n"
    ) % (boundary, field, filename)
    tail = "\r\n--%s--\r\n" % boundary
    return head.encode("utf-8") + content + tail.encode("utf-8")


def main_with_cases(cases):
    failures = 0
    for name, func in cases:
        if not run_case(name, func):
            failures += 1
    if failures:
        print("[SUMMARY] %d failed" % failures)
        return 1
    print("[SUMMARY] all passed")
    return 0


if __name__ == "__main__":
    sys.exit(0)
