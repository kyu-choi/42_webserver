#!/usr/bin/env python3
import re
import sys
import time

from http_utils import (
    Server,
    TestFailure,
    assert_contains,
    assert_status,
    main_with_cases,
    request,
)


SESSION_RE = re.compile(r"WSID=([0-9a-f]{32})")


def cookie_from_response(response):
    value = response.headers.get("set-cookie", "")
    match = SESSION_RE.search(value)
    if not match:
        raise TestFailure("missing WSID Set-Cookie header: %r" % value)
    return match.group(1)


def body_value(response, key):
    prefix = (key + ": ").encode("ascii")
    for line in response.body.splitlines():
        if line.startswith(prefix):
            return line[len(prefix):].decode("ascii")
    raise TestFailure("missing %s in body\n%s" % (key, response.text()))


def cookie_session_flow():
    with Server("config/step20.conf", ports=(8080, 8081)):
        static = request("GET", "/")
        assert_status(static, 200, "static route before session")
        if "set-cookie" in static.headers:
            raise TestFailure("static route should not create a session")

        first = request("GET", "/session")
        assert_status(first, 200, "first session")
        sid = cookie_from_response(first)
        assert_contains(first, "new: yes", "first session new flag")
        assert_contains(first, "visits: 1", "first session visits")

        second = request("GET", "/session", headers={"Cookie": "WSID=%s" % sid})
        assert_status(second, 200, "second session")
        assert_contains(second, "new: no", "second session reused")
        assert_contains(second, "visits: 2", "second session visits")

        bad = request("GET", "/session", headers={"Cookie": "WSID=../../bad"})
        assert_status(bad, 200, "bad cookie ignored")
        assert_contains(bad, "new: yes", "bad cookie replaced")


def session_ttl_and_limit():
    with Server("config/step20.conf", ports=(8080, 8081)):
        first = request("GET", "/session")
        sid = cookie_from_response(first)
        time.sleep(4)
        expired = request("GET", "/session", headers={"Cookie": "WSID=%s" % sid})
        assert_status(expired, 200, "expired session")
        assert_contains(expired, "new: yes", "expired session replaced")

        last = None
        for _ in range(70):
            last = request("GET", "/session")
            assert_status(last, 200, "session limit create")
        active = int(body_value(last, "active_sessions"))
        if active > 64:
            raise TestFailure("session limit exceeded: %d" % active)


def multiple_cgi_types():
    with Server("config/step20.conf", ports=(8080, 8081)):
        py = request("GET", "/cgi-bin/hello.py?name=python")
        assert_status(py, 200, "python cgi")
        assert_contains(py, "method=GET", "python cgi body")

        sh = request("GET", "/cgi-bin/hello.sh?name=shell")
        assert_status(sh, 200, "shell cgi get")
        assert_contains(sh, "cgi-type: shell", "shell cgi marker")
        assert_contains(sh, "method: GET", "shell cgi method")
        assert_contains(sh, "query: name=shell", "shell cgi query")

        posted = request("POST", "/cgi-bin/hello.sh", body="bonus body")
        assert_status(posted, 200, "shell cgi post")
        assert_contains(posted, "method: POST", "shell cgi post method")
        assert_contains(posted, "body: bonus body", "shell cgi post body")


if __name__ == "__main__":
    sys.exit(main_with_cases([
        ("cookie session flow", cookie_session_flow),
        ("session ttl and limit", session_ttl_and_limit),
        ("multiple CGI types", multiple_cgi_types),
    ]))
