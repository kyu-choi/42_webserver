#!/usr/bin/env python3
import shutil
import subprocess
import sys

from http_utils import ROOT, Server, TestFailure, assert_status, request


def valgrind_smoke():
    if shutil.which("valgrind") is None:
        print("[INFO] valgrind not found; skipping")
        return
    command = [
        "valgrind",
        "--leak-check=full",
        "--show-leak-kinds=all",
        "--track-fds=yes",
        str(ROOT / "webserv"),
        str(ROOT / "config" / "step18.conf"),
    ]
    with Server("config/step18.conf", ports=(8080, 8081), command=command) as server:
        assert_status(request("GET", "/"), 200, "valgrind static")
        assert_status(request("GET", "/cgi-bin/hello.py?vg=1", timeout=8), 200, "valgrind cgi")
        assert_status(request("GET", "/cgi-bin/timeout.py", timeout=10), 504, "valgrind cgi timeout")
    output = server.output
    if "All heap blocks were freed -- no leaks are possible" not in output:
        raise TestFailure("valgrind did not report all heap blocks freed\n" + output[-1200:])
    if "FILE DESCRIPTORS: 3 open (3 std) at exit." not in output:
        raise TestFailure("valgrind fd summary was not clean\n" + output[-1200:])
    if "ERROR SUMMARY: 0 errors" not in output:
        raise TestFailure("valgrind reported errors\n" + output[-1200:])


if __name__ == "__main__":
    try:
        valgrind_smoke()
        print("[PASS] valgrind smoke")
        sys.exit(0)
    except Exception as error:
        print("[FAIL] valgrind smoke: %s" % error)
        sys.exit(1)
