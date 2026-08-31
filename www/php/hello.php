<?php
header("Content-Type: text/plain");
echo "Hello from PHP CGI!\n";
echo "QUERY_STRING = " . ($_SERVER['QUERY_STRING'] ?? '') . "\n";
echo "REQUEST_METHOD = " . ($_SERVER['REQUEST_METHOD'] ?? '') . "\n";
echo "SCRIPT_NAME = " . ($_SERVER['SCRIPT_NAME'] ?? '') . "\n";
echo "REDIRECT_STATUS = " . ($_SERVER['REDIRECT_STATUS'] ?? '') . "\n";
