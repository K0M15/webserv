#!/usr/bin/env python3
"""Chunked transfer-encoding request test for the webserv app.

Start the server first, from the repository root:

    ./webserv www/test/test.conf

then run this script:

    python3 www/test/chunked_test.py [host] [port]
"""
import http.client
import socket
import sys

HOST = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
PORT = int(sys.argv[2]) if len(sys.argv) > 2 else 8080

CGI = "/test/cgi/python/echo.py"
EXPECTED_BODY = "hello chunked world"


def fetch_chunked():
    """POST via http.client with an iterable body -> Transfer-Encoding: chunked.

    http.client takes any body without a __len__ as chunked, so the bytes
    arrive at the server framed with Transfer-Encoding: chunked.
    """
    def gen():
        for part in (b"hello ", b"chunked ", b"world"):
            yield part

    conn = http.client.HTTPConnection(HOST, PORT, timeout=5)
    try:
        conn.request("POST", CGI, body=gen(),
                     headers={"Content-Type": "text/plain"})
        resp = conn.getresponse()
        body = resp.read().decode("utf-8", errors="replace")
        return resp.status, body
    finally:
        conn.close()


def fetch_content_length():
    """Regression check: a normal Content-Length POST still works."""
    conn = http.client.HTTPConnection(HOST, PORT, timeout=5)
    try:
        conn.request("POST", CGI, body=EXPECTED_BODY,
                     headers={"Content-Type": "text/plain"})
        resp = conn.getresponse()
        body = resp.read().decode("utf-8", errors="replace")
        return resp.status, body
    finally:
        conn.close()


def raw_request(payload):
    """Send raw bytes over a fresh socket and return (status, body)."""
    s = socket.create_connection((HOST, PORT), timeout=5)
    try:
        s.sendall(payload)
        data = b""
        while True:
            chunk = s.recv(4096)
            if not chunk:
                break
            data += chunk
    finally:
        s.close()
    head, _, body = data.partition(b"\r\n\r\n")
    parts = head.split(b" ", 2)
    status = int(parts[1]) if len(parts) > 1 else 0
    return status, body.decode("utf-8", errors="replace")


def make_payload(extra_headers, body_bytes):
    req = f"POST {CGI} HTTP/1.1\r\nHost: {HOST}:{PORT}\r\n"
    req += extra_headers + "\r\n"
    return req.encode() + body_bytes


def chunked_body(chunks):
    out = b""
    for c in chunks:
        out += f"{len(c):x}\r\n".encode() + c + b"\r\n"
    return out + b"0\r\n\r\n"


def main():
    passed = 0
    failed = 0

    def record(name, ok, detail=""):
        nonlocal passed, failed
        if ok:
            print(f"[SUCCESS] {name}")
            passed += 1
        else:
            print(f"[FAILURE] {name} - {detail}")
            failed += 1

    print(f"Testing chunked requests against http://{HOST}:{PORT}\n")

    try:
        status, body = fetch_chunked()
    except Exception as e:
        record("chunked POST de-chunked by CGI", False,
               f"connection error: {e}")
        print(f"\n{'=' * 40}\nResults: {passed} passed, {failed} failed")
        return 1
    record("chunked POST de-chunked by CGI",
           status == 200 and f"BODY={EXPECTED_BODY}" in body,
           f"status {status}, body={body!r}")

    try:
        status, body = fetch_content_length()
    except Exception as e:
        record("Content-Length POST regression", False,
               f"connection error: {e}")
        print(f"\n{'=' * 40}\nResults: {passed} passed, {failed} failed")
        return 1
    record("Content-Length POST regression",
           status == 200 and f"BODY={EXPECTED_BODY}" in body,
           f"status {status}, body={body!r}")

    # Request smuggling: Content-Length and Transfer-Encoding together -> 400.
    status, body = raw_request(make_payload(
        "Content-Type: text/plain\r\n"
        f"Content-Length: {len(EXPECTED_BODY)}\r\n"
        "Transfer-Encoding: chunked\r\n",
        chunked_body([b"hello ", b"chunked ", b"world"])))
    record("Content-Length + Transfer-Encoding rejected (400)",
           status == 400, f"status {status}")

    # Malformed chunk size -> 400.
    status, body = raw_request(make_payload(
        "Content-Type: text/plain\r\nTransfer-Encoding: chunked\r\n",
        b"zz\r\nxxxxx\r\n0\r\n\r\n"))
    record("malformed chunk size rejected (400)",
           status == 400, f"status {status}")

    # Decoded body exceeds max_body_size (4096) -> 413.
    status, body = raw_request(make_payload(
        "Content-Type: text/plain\r\nTransfer-Encoding: chunked\r\n",
        chunked_body([b"A" * 4096, b"X"])))
    record("oversized chunked body rejected (413)",
           status == 413, f"status {status}")

    print(f"\n{'=' * 40}")
    print(f"Results: {passed} passed, {failed} failed")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
