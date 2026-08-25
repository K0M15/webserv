#!/usr/bin/env python3
import os
import sys

body = sys.stdin.read()
sys.stdout.write("Content-Type: text/plain\r\n\r\n")
sys.stdout.write("QUERY=%s\n" % os.environ.get("QUERY_STRING", ""))
sys.stdout.write("BODY=%s\n" % body)
sys.stdout.write("TE=%s\n" % os.environ.get("HTTP_TRANSFER_ENCODING", ""))
