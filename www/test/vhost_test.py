#!/usr/bin/env python3
"""Virtual host routing test for the webserv app.

Start the server first, from the repository root:

    ./webserv www/test/test.conf

then run this script:

    python3 www/test/vhost_test.py [host] [port]
"""
import http.client
import sys

HOST = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
PORT = int(sys.argv[2]) if len(sys.argv) > 2 else 8080

DEFAULT_MARKER = "Hello from webserv!"   # www/index.html (default server root)
VHOST_MARKER = "Virtual host root"       # www/test/vhost/index.html (vhost root)

CASES = [
    # (Host header, marker expected, description)
    ("localhost:8080",  DEFAULT_MARKER, "default server (Host localhost:8080)"),
    ("localhost",       DEFAULT_MARKER, "default server (Host localhost)"),
    ("vhost.test:8080", VHOST_MARKER,   "vhost (Host vhost.test:8080)"),
    ("VHOST.TEST:8080", VHOST_MARKER,   "vhost (case-insensitive Host)"),
    ("vhost.test",      VHOST_MARKER,   "vhost (Host without port)"),
    ("unknown.example", DEFAULT_MARKER, "unknown Host falls back to default"),
    (None,              DEFAULT_MARKER, "no Host header falls back to default"),
]


def fetch(host_header, path="/"):
    conn = http.client.HTTPConnection(HOST, PORT, timeout=5)
    try:
        conn.putrequest("GET", path, skip_host=True, skip_accept_encoding=True)
        if host_header is not None:
            conn.putheader("Host", host_header)
        conn.endheaders()
        resp = conn.getresponse()
        body = resp.read().decode("utf-8", errors="replace")
        return resp.status, body
    finally:
        conn.close()


def main():
    passed = 0
    failed = 0

    print(f"Testing virtual hosts against http://{HOST}:{PORT}\n")
    for host_header, marker, desc in CASES:
        try:
            status, body = fetch(host_header)
        except Exception as e:
            print(f"[FAILURE] {desc} - connection error: {e}")
            failed += 1
            continue

        ok = status == 200 and marker in body
        if ok:
            print(f"[SUCCESS] {desc}")
            passed += 1
        else:
            print(f"[FAILURE] {desc} - got status {status}, expected 200 "
                  f"with marker {marker!r}")
            failed += 1

    print(f"\n{'=' * 40}")
    print(f"Results: {passed} passed, {failed} failed")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
