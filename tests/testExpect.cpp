#include <iostream>
#include <cassert>
#include <sstream>
#include <vector>
#include <string>
#include "CGIHandler.hpp"

#define private public
#include "ConnectionManager.hpp"
#include "WebserverSettings.hpp"
#include "Connection.hpp"

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

// Setup dummy connection
static Connection createDummyConnection(const std::string& raw_request, size_t max_body_size = 1000) {
    sockaddr_in addr{};
    WebserverSettings* settings = new WebserverSettings();
    settings->max_body_size = max_body_size;
    std::vector<const WebserverSettings*> candidates = { settings };

    Connection conn(-1, addr, candidates);
    conn.settings = settings;
    conn.read_buffer = raw_request;
    return conn;
}

static void test_expect_payload_too_large() {
    std::string raw =
        "POST /upload HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 5000\r\n"
        "Expect: 100-continue\r\n"
        "\r\n";
    ConnectionManager cm;
    Connection conn = createDummyConnection(raw, 1000); // limit is 1000, body length is 5000
    RequestReadState state = cm.isRequestComplete(conn);

    check("Expect: 100-continue respects max_body_size (413 Payload Too Large)",
          state == RequestReadState::PAYLOAD_TOO_LARGE);
    delete conn.settings;
}

static void test_expect_same_time_headers_and_payload() {
    std::string raw =
        "POST /upload HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 5\r\n"
        "Expect: 100-continue\r\n"
        "\r\n"
        "hello";
    ConnectionManager cm;
    Connection conn = createDummyConnection(raw, 1000);
    RequestReadState state = cm.isRequestComplete(conn);

    check("Expect: 100-continue returns COMPLETE when header & payload arrive at same time",
          state == RequestReadState::COMPLETE);
    delete conn.settings;
}

static void test_expect_partial_payload() {
    std::string raw =
        "POST /upload HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 10\r\n"
        "Expect: 100-continue\r\n"
        "\r\n"
        "hel"; // only 3 of 10 bytes
    ConnectionManager cm;
    Connection conn = createDummyConnection(raw, 1000);
    RequestReadState state = cm.isRequestComplete(conn);

    check("Expect: 100-continue returns INCOMPLETE for partial payload",
          state == RequestReadState::INCOMPLETE);
    delete conn.settings;
}

static void test_expect_invalid_expectation() {
    std::string raw =
        "POST /upload HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 5\r\n"
        "Expect: 200-ok\r\n"
        "\r\n";
    ConnectionManager cm;
    Connection conn = createDummyConnection(raw, 1000);
    RequestReadState state = cm.isRequestComplete(conn);

    check("Expect: unsupported expectation returns EXPECTATION_FAILED (417)",
          state == RequestReadState::EXPECTATION_FAILED);
    delete conn.settings;
}

int main() {
    std::cout << "--- Expect: 100-continue Test Suite ---\n" << std::endl;

    test_expect_payload_too_large();
    test_expect_same_time_headers_and_payload();
    test_expect_partial_payload();
    test_expect_invalid_expectation();

    std::cout << "\n----------------------------------------" << std::endl;
    std::cout << "Results: " << g_passed << " passed, " << g_failed << " failed, "
              << (g_passed + g_failed) << " total" << std::endl;

    return g_failed > 0 ? 1 : 0;
}
