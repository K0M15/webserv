#!/usr/bin/env python3
"""Simple CGI script: echoes request info as text."""
import os

print("Content-Type: text/plain")
print("")
print("Hello from Python CGI!")
print("QUERY_STRING = " + os.environ.get("QUERY_STRING", ""))
print("REQUEST_METHOD = " + os.environ.get("REQUEST_METHOD", ""))
print("SCRIPT_NAME = " + os.environ.get("SCRIPT_NAME", ""))
print("PATH_INFO = " + os.environ.get("PATH_INFO", ""))
print("HTTP_USER_AGENT = " + os.environ.get("HTTP_USER_AGENT", ""))
