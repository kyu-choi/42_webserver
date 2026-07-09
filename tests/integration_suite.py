#!/usr/bin/env python3
import sys
from pathlib import Path

from http_utils import (
    ROOT,
    Server,
    TestFailure,
    assert_contains,
    assert_header,
    assert_status,
    cleanup_uploads,
    main_with_cases,
    multipart_body,
    request,
)


def static_and_route_features():
    with Server("config/step18.conf", ports=(8080, 8081)):
        root = request("GET", "/")
        assert_status(root, 200, "root")
        assert_contains(root, "webserv static file server", "root body")
        assert_header(root, "content-type", "text/html", "root content-type")

        css = request("GET", "/style.css")
        assert_status(css, 200, "css")
        assert_header(css, "content-type", "text/css", "css content-type")

        logo = request("GET", "/images/logo.svg")
        assert_status(logo, 200, "svg")
        expected_logo = (ROOT / "www" / "images" / "logo.svg").read_bytes()
        if logo.body != expected_logo:
            raise TestFailure("svg response differs from source file")

        missing = request("GET", "/no-such-file")
        assert_status(missing, 404, "missing file")

        traversal = request("GET", "/../../etc/passwd")
        assert_status(traversal, 403, "traversal")

        redirect = request("GET", "/old")
        assert_status(redirect, 301, "redirect")
        assert_header(redirect, "location", "/", "redirect location")

        method = request("GET", "/post-only")
        assert_status(method, 405, "method not allowed")
        assert_header(method, "allow", "POST", "allow header")

        not_implemented = request("PUT", "/")
        assert_status(not_implemented, 501, "not implemented method")

        index = request("GET", "/has-index/")
        assert_status(index, 200, "index precedence")
        assert_contains(index, "has-index home", "index precedence body")

        listing = request("GET", "/listing/")
        assert_status(listing, 200, "autoindex on")
        assert_contains(listing, "a.txt", "autoindex body")
        assert_contains(listing, "b.txt", "autoindex body")

        private = request("GET", "/private-directory/")
        assert_status(private, 403, "autoindex off")


def upload_and_delete_features():
    cleanup_uploads()
    try:
        with Server("config/step18.conf", ports=(8080, 8081)):
            boundary = "step18boundary"
            body = multipart_body(boundary, "step18-upload.txt", "first")
            response = request(
                "POST",
                "/upload",
                headers={"Content-Type": "multipart/form-data; boundary=%s" % boundary},
                body=body,
            )
            assert_status(response, 201, "multipart upload create")
            assert_contains(response, "created step18-upload.txt", "upload create body")

            body = multipart_body(boundary, "step18-upload.txt", "second")
            response = request(
                "POST",
                "/upload",
                headers={"Content-Type": "multipart/form-data; boundary=%s" % boundary},
                body=body,
            )
            assert_status(response, 200, "multipart upload overwrite")
            assert_contains(response, "updated step18-upload.txt", "upload overwrite body")

            downloaded = request("GET", "/uploads/step18-upload.txt")
            assert_status(downloaded, 200, "download uploaded text")
            if downloaded.body != b"second":
                raise TestFailure("uploaded text content mismatch")

            binary = b"\x00\x01binary\xff\n"
            body = multipart_body(boundary, "step18-binary.bin", binary)
            response = request(
                "POST",
                "/upload",
                headers={"Content-Type": "multipart/form-data; boundary=%s" % boundary},
                body=body,
            )
            assert_status(response, 201, "multipart binary upload")
            downloaded = request("GET", "/uploads/step18-binary.bin")
            assert_status(downloaded, 200, "download uploaded binary")
            if downloaded.body != binary:
                raise TestFailure("uploaded binary content mismatch")

            bad = multipart_body(boundary, "../evil.txt", "bad")
            response = request(
                "POST",
                "/upload",
                headers={"Content-Type": "multipart/form-data; boundary=%s" % boundary},
                body=bad,
            )
            assert_status(response, 400, "malicious filename")

            deleted = request("DELETE", "/delete/step18-upload.txt")
            assert_status(deleted, 204, "delete upload")
            missing = request("DELETE", "/delete/step18-upload.txt")
            assert_status(missing, 404, "delete missing upload")
            directory = request("DELETE", "/delete/")
            assert_status(directory, 403, "delete directory")
            traversal = request("DELETE", "/delete/../README.md")
            assert_status(traversal, 403, "delete traversal")
    finally:
        cleanup_uploads()


if __name__ == "__main__":
    sys.exit(main_with_cases([
        ("static, routing, errors, autoindex", static_and_route_features),
        ("upload and delete", upload_and_delete_features),
    ]))
