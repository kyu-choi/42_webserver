#!/usr/bin/env python3
import subprocess
import sys
from pathlib import Path

from http_utils import ROOT, WEBSERV, Server, TestFailure, request, assert_status, assert_contains, main_with_cases


def expect_bad_config(path):
    result = subprocess.run(
        [str(WEBSERV), str(path)],
        cwd=str(ROOT),
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        timeout=3,
    )
    if result.returncode == 0:
        raise TestFailure("config unexpectedly succeeded: %s" % path)


def invalid_configs_rejected():
    expect_bad_config(ROOT / "config" / "missing-does-not-exist.conf")
    for path in sorted((ROOT / "config" / "invalid").glob("*.conf")):
        expect_bad_config(path)


def normal_configs_start():
    configs = [
        ("config/default.conf", (8080,)),
        ("config/step18.conf", (8080, 8081)),
        ("config/step19.conf", (8080, 8081)),
        ("config/step20.conf", (8080, 8081)),
        ("config/multiple_ports.conf", (8080, 8081)),
    ]
    for config, ports in configs:
        with Server(config, ports=ports):
            response = request("GET", "/", port=ports[0])
            assert_status(response, 200, config)


def multiple_ports_have_different_bodies():
    with Server("config/step18.conf", ports=(8080, 8081)):
        first = request("GET", "/", port=8080)
        second = request("GET", "/", port=8081)
        assert_status(first, 200, "8080 root")
        assert_status(second, 200, "8081 root")
        assert_contains(first, "webserv static file server", "8080 body")
        assert_contains(second, "site_b index", "8081 body")


if __name__ == "__main__":
    sys.exit(main_with_cases([
        ("invalid configs are rejected", invalid_configs_rejected),
        ("normal configs start", normal_configs_start),
        ("multiple ports serve different bodies", multiple_ports_have_different_bodies),
    ]))
