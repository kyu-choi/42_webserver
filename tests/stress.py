#!/usr/bin/env python3
import concurrent.futures
import sys

from http_utils import Response, Server, assert_status, main_with_cases, raw_request, request


def static_task(index):
    port = 8080 if index % 2 == 0 else 8081
    response = request("GET", "/", port=port, timeout=5)
    assert_status(response, 200, "stress static %d" % index)
    return True


def mixed_task(index):
    if index % 5 == 0:
        response = Response(raw_request(b"BAD\r\n\r\n", timeout=5))
        assert_status(response, 400, "stress malformed %d" % index)
    elif index % 5 == 1:
        response = request("GET", "/cgi-bin/hello.py?i=%d" % index, timeout=5)
        assert_status(response, 200, "stress cgi %d" % index)
    elif index % 5 == 2:
        response = request("GET", "/style.css", timeout=5)
        assert_status(response, 200, "stress css %d" % index)
    else:
        response = request("GET", "/", port=8080 if index % 2 == 0 else 8081, timeout=5)
        assert_status(response, 200, "stress root %d" % index)
    return True


def run_concurrent(count, worker, workers=32):
    with concurrent.futures.ThreadPoolExecutor(max_workers=workers) as pool:
        futures = [pool.submit(worker, i) for i in range(count)]
        for future in concurrent.futures.as_completed(futures):
            future.result()


def stress_requests():
    with Server("config/step18.conf", ports=(8080, 8081)):
        run_concurrent(120, static_task, workers=40)
        run_concurrent(80, mixed_task, workers=32)
        alive = request("GET", "/")
        assert_status(alive, 200, "server alive after stress")


if __name__ == "__main__":
    sys.exit(main_with_cases([
        ("stress requests", stress_requests),
    ]))
