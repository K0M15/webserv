<?php
header("Content-Type: text/plain");
echo "Hello from PHP CGI!\n";
echo "QUERY=" . (isset($_SERVER['QUERY_STRING']) ? $_SERVER['QUERY_STRING'] : '') . "\n";
echo "BODY=" . file_get_contents("php://input") . "\n";
echo "TE=" . (isset($_SERVER['HTTP_TRANSFER_ENCODING']) ? $_SERVER['HTTP_TRANSFER_ENCODING'] : '') . "\n";
?>
