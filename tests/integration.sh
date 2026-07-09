#!/usr/bin/env bash

set -u

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT_DIR" || exit 1

failures=0

run_step() {
    name="$1"
    shift
    printf '[RUN] %s\n' "$name"
    "$@"
    status=$?
    if [ "$status" -eq 0 ]; then
        printf '[PASS] %s\n' "$name"
    else
        printf '[FAIL] %s (exit %d)\n' "$name" "$status"
        failures=$((failures + 1))
    fi
}

check_make_no_relink() {
    output="$(make 2>&1)"
    printf '%s\n' "$output"
    printf '%s\n' "$output" | grep -q "Nothing to be done"
}

check_binary_name() {
    test -x ./webserv
}

check_cxx98_flags() {
    grep -q -- "-std=c++98" Makefile
}

run_step "make re" make re
run_step "make no unnecessary relink" check_make_no_relink
run_step "binary name is webserv" check_binary_name
run_step "C++98 flags are present" check_cxx98_flags
run_step "config suite" python3 tests/config_suite.py
run_step "integration suite" python3 tests/integration_suite.py
run_step "malformed suite" python3 tests/malformed.py
run_step "chunked suite" python3 tests/chunked.py
run_step "slow client suite" python3 tests/slow_client.py
run_step "CGI non-blocking suite" python3 tests/cgi_nonblocking.py
run_step "stress suite" python3 tests/stress.py

if [ "${SKIP_VALGRIND:-0}" = "1" ]; then
    printf '[INFO] valgrind smoke skipped because SKIP_VALGRIND=1\n'
else
    run_step "valgrind smoke" python3 tests/valgrind_smoke.py
fi

if [ "$failures" -ne 0 ]; then
    printf '[SUMMARY] %d step(s) failed\n' "$failures"
    exit 1
fi

printf '[SUMMARY] all integration checks passed\n'
