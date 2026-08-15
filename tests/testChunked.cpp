#include "Chunked.hpp"
#include "Request.hpp"
#include <iostream>
#include <sstream>
#include <string>

static int g_passed = 0;
static int g_failed = 0;

static void pass(const char* name) {
    std::cout << "[SUCCESS] " << name << std::endl;
    ++g_passed;
}

static void fail(const char* name, const char* msg) {
    std::cout << "[FAILURE] " << name;
    if (msg && msg[0]) std::cout << " - " << msg;
    std::cout << std::endl;
    ++g_failed;
}

static void check(const char* name, bool condition, const char* fail_msg = "") {
    if (condition)
        pass(name);
    else
        fail(name, fail_msg);
}

// --------------- ChunkedBody::decode ---------------

static void test_decode_basic() {
    std::string decoded;
    size_t consumed = 0;

    ChunkResult r = ChunkedBody::decode("5\r\nhello\r\n0\r\n\r\n", 100, decoded, consumed);
    check("basic: single chunk COMPLETE", r == ChunkResult::COMPLETE);
    check("basic: decoded content", decoded == "hello", decoded.c_str());
    check("basic: consumed size", consumed == 15, "expected 15 bytes framed");

    r = ChunkedBody::decode("0\r\n\r\n", 100, decoded, consumed);
    check("basic: empty body COMPLETE", r == ChunkResult::COMPLETE);
    check("basic: empty decoded", decoded.empty());
    check("basic: empty consumed", consumed == 5);

    r = ChunkedBody::decode("4\r\nWiki\r\n5\r\npedia\r\n0\r\n\r\n", 100, decoded, consumed);
    check("basic: multi chunk COMPLETE", r == ChunkResult::COMPLETE);
    check("basic: multi chunk decoded", decoded == "Wikipedia", decoded.c_str());
    check("basic: multi chunk consumed", consumed == 24);
}

static void test_decode_variants() {
    std::string decoded;
    size_t consumed = 0;

    ChunkResult r = ChunkedBody::decode("A\r\nhelloWorld\r\n0\r\n\r\n", 100, decoded, consumed);
    check("variants: uppercase hex", r == ChunkResult::COMPLETE && decoded == "helloWorld", decoded.c_str());

    r = ChunkedBody::decode("5;ext=value\r\nhello\r\n0\r\n\r\n", 100, decoded, consumed);
    check("variants: chunk extension ignored", r == ChunkResult::COMPLETE && decoded == "hello", decoded.c_str());

    r = ChunkedBody::decode("5\r\nhello\r\n0\r\nX-Trailer: yes\r\n\r\n", 100, decoded, consumed);
    check("variants: trailer section skipped", r == ChunkResult::COMPLETE && decoded == "hello", decoded.c_str());

    r = ChunkedBody::decode("1\r\na\r\n1\r\na\r\n1\r\na\r\n0\r\n\r\n", 100, decoded, consumed);
    check("variants: many small chunks", r == ChunkResult::COMPLETE && decoded == "aaa", decoded.c_str());
}

static void test_decode_incomplete() {
    std::string decoded;
    size_t consumed = 0;

    ChunkResult r = ChunkedBody::decode("5\r\nhel", 100, decoded, consumed);
    check("incomplete: chunk data short", r == ChunkResult::INCOMPLETE);

    r = ChunkedBody::decode("5\r\nhello\r\n", 100, decoded, consumed);
    check("incomplete: terminator missing", r == ChunkResult::INCOMPLETE);

    r = ChunkedBody::decode("5\r\nhello\r\n0\r\n", 100, decoded, consumed);
    check("incomplete: final CRLF missing", r == ChunkResult::INCOMPLETE);

    r = ChunkedBody::decode("5", 100, decoded, consumed);
    check("incomplete: no line end", r == ChunkResult::INCOMPLETE);
}

static void test_decode_malformed() {
    std::string decoded;
    size_t consumed = 0;

    ChunkResult r = ChunkedBody::decode("zz\r\nfoo\r\n0\r\n\r\n", 100, decoded, consumed);
    check("malformed: non-hex size", r == ChunkResult::MALFORMED);

    r = ChunkedBody::decode("\r\nfoo\r\n0\r\n\r\n", 100, decoded, consumed);
    check("malformed: empty size line", r == ChunkResult::MALFORMED);

    r = ChunkedBody::decode("5\r\nhelloXX0\r\n\r\n", 100, decoded, consumed);
    check("malformed: missing CRLF after data", r == ChunkResult::MALFORMED);

    r = ChunkedBody::decode("5\r\nhello\r\n0\r\nbad trailer", 100, decoded, consumed);
    check("malformed: unterminated trailers", r == ChunkResult::INCOMPLETE);
}

static void test_decode_too_large() {
    std::string decoded;
    size_t consumed = 0;

    ChunkResult r = ChunkedBody::decode("6\r\nabcdef\r\n0\r\n\r\n", 5, decoded, consumed);
    check("too_large: over limit", r == ChunkResult::TOO_LARGE);

    r = ChunkedBody::decode("6\r\nabcdef\r\n0\r\n\r\n", 6, decoded, consumed);
    check("too_large: at limit", r == ChunkResult::COMPLETE && decoded == "abcdef", decoded.c_str());

    r = ChunkedBody::decode("5\r\nabcde\r\n5\r\nfghij\r\n0\r\n\r\n", 7, decoded, consumed);
    check("too_large: cumulative over limit", r == ChunkResult::TOO_LARGE);

    r = ChunkedBody::decode("100000000000000000000000000000000\r\nx\r\n0\r\n\r\n", 100, decoded, consumed);
    check("too_large: huge hex size", r == ChunkResult::TOO_LARGE);
}

// --------------- Request::fromString ---------------

static std::string makeRequest(const std::string& headers, const std::string& body) {
    return "POST /upload/test.txt HTTP/1.1\r\n" + headers + "\r\n" + body;
}

static void test_request_chunked() {
    std::string raw = makeRequest(
        "Host: localhost:8080\r\n"
        "Content-Type: application/octet-stream\r\n"
        "Transfer-Encoding: chunked\r\n",
        "4\r\nWiki\r\n5\r\npedia\r\n0\r\n\r\n");

    Request req = Request::fromString(raw);
    check("request: chunked body decoded", req.getBody() == "Wikipedia", req.getBody().c_str());
    check("request: isChunked() true", req.isChunked());
}

static void test_request_chunked_uppercase() {
    std::string raw = makeRequest(
        "Host: localhost:8080\r\n"
        "Transfer-Encoding: CHUNKED\r\n",
        "5\r\nhello\r\n0\r\n\r\n");

    Request req = Request::fromString(raw);
    check("request: TE header case-insensitive", req.getBody() == "hello" && req.isChunked(), req.getBody().c_str());
}

static void test_request_chunked_empty() {
    std::string raw = makeRequest(
        "Host: localhost:8080\r\n"
        "Transfer-Encoding: chunked\r\n",
        "0\r\n\r\n");

    Request req = Request::fromString(raw);
    check("request: chunked empty body", req.getBody().empty() && req.isChunked());
}

static void test_request_content_length() {
    std::string raw = makeRequest(
        "Host: localhost:8080\r\n"
        "Content-Length: 5\r\n",
        "hello");

    Request req = Request::fromString(raw);
    check("request: content-length body", req.getBody() == "hello", req.getBody().c_str());
    check("request: content-length not chunked", !req.isChunked());
}

static void test_request_smuggling() {
    std::string raw = makeRequest(
        "Host: localhost:8080\r\n"
        "Content-Length: 5\r\n"
        "Transfer-Encoding: chunked\r\n",
        "5\r\nhello\r\n0\r\n\r\n");

    bool threw = false;
    try {
        Request::fromString(raw);
    } catch (const std::exception&) {
        threw = true;
    }
    check("request: CL + TE rejected", threw, "expected exception");
}

static void test_request_unsupported_coding() {
    std::string raw = makeRequest(
        "Host: localhost:8080\r\n"
        "Transfer-Encoding: gzip\r\n",
        "garbage");

    bool threw = false;
    try {
        Request::fromString(raw);
    } catch (const std::exception&) {
        threw = true;
    }
    check("request: unsupported coding rejected", threw, "expected exception");
}

static void test_request_malformed_chunked() {
    std::string raw = makeRequest(
        "Host: localhost:8080\r\n"
        "Transfer-Encoding: chunked\r\n",
        "zz\r\nfoo\r\n0\r\n\r\n");

    bool threw = false;
    try {
        Request::fromString(raw);
    } catch (const std::exception&) {
        threw = true;
    }
    check("request: malformed chunked rejected", threw, "expected exception");
}

int main() {
    std::cout << "--- Chunked Tests ---\n" << std::endl;

    std::cout << "ChunkedBody::decode basic:" << std::endl;
    test_decode_basic();
    std::cout << std::endl;

    std::cout << "ChunkedBody::decode variants:" << std::endl;
    test_decode_variants();
    std::cout << std::endl;

    std::cout << "ChunkedBody::decode incomplete:" << std::endl;
    test_decode_incomplete();
    std::cout << std::endl;

    std::cout << "ChunkedBody::decode malformed:" << std::endl;
    test_decode_malformed();
    std::cout << std::endl;

    std::cout << "ChunkedBody::decode size limit:" << std::endl;
    test_decode_too_large();
    std::cout << std::endl;

    std::cout << "Request::fromString integration:" << std::endl;
    test_request_chunked();
    test_request_chunked_uppercase();
    test_request_chunked_empty();
    test_request_content_length();
    test_request_smuggling();
    test_request_unsupported_coding();
    test_request_malformed_chunked();
    std::cout << std::endl;

    std::cout << "----------------------------------------" << std::endl;
    std::cout << "Results: " << g_passed << " passed, " << g_failed << " failed, "
              << (g_passed + g_failed) << " total" << std::endl;
    return g_failed > 0 ? 1 : 0;
}
