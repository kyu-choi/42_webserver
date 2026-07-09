import os
import sys

body = sys.stdin.read()

print("Content-Type: text/plain")
print("Status: 200 OK")
print()
print("method=" + os.environ.get("REQUEST_METHOD", ""))
print("query=" + os.environ.get("QUERY_STRING", ""))
print("content_length=" + os.environ.get("CONTENT_LENGTH", ""))
print("content_type=" + os.environ.get("CONTENT_TYPE", ""))
print("script_name=" + os.environ.get("SCRIPT_NAME", ""))
print("script_filename=" + os.environ.get("SCRIPT_FILENAME", ""))
print("http_host=" + os.environ.get("HTTP_HOST", ""))
print("body=" + body)
