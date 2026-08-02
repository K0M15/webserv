#!/usr/bin/env python3
"""CGI script for tests/testCGI.cpp.

Echoes every environment variable set for the CGI process, sorted for
deterministic output, followed by the raw stdin body. Output is
CGI-formatted (header block, blank line, then the body).
"""
import os
import sys


def main():
    lines = ["Content-Type: text/plain", ""]
    for key in sorted(os.environ):
        lines.append("%s=%s" % (key, os.environ[key]))
    lines.append("")
    lines.append("[stdin]")
    lines.append(sys.stdin.read())
    sys.stdout.write("\n".join(lines))


if __name__ == "__main__":
    main()
