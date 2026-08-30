#include <iostream>
#include <cassert>
#include <sstream>
#include <vector>
#include <string>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include "Request.hpp"
#include "WebserverSettings.hpp"
#include "Connection.hpp"
#include "PollHandler.hpp"

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
    Connection conn = createDummyConnection(raw, 1000); // limit is 1000, body length is 5000
    RequestReadState state = Request::isRequestComplete(conn);

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
    Connection conn = createDummyConnection(raw, 1000);
    RequestReadState state = Request::isRequestComplete(conn);

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
    Connection conn = createDummyConnection(raw, 1000);
    RequestReadState state = Request::isRequestComplete(conn);

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
    Connection conn = createDummyConnection(raw, 1000);
    RequestReadState state = Request::isRequestComplete(conn);

    check("Expect: unsupported expectation returns EXPECTATION_FAILED (417)",
          state == RequestReadState::EXPECTATION_FAILED);
    delete conn.settings;
}

static void test_chunked_keep_alive_buffer_cleanup() {
    std::string first_chunked =
        "POST /upload HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "5\r\nhello\r\n0\r\n\r\n";
    std::string second_request =
        "GET /index.html HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n";

    Connection conn = createDummyConnection(first_chunked + second_request, 1000);
    RequestReadState state = Request::isRequestComplete(conn);

    check("Chunked: isRequestComplete returns COMPLETE for pipelined chunked request",
          state == RequestReadState::COMPLETE);
    check("Chunked: raw_body_length matches raw chunk framing size (15 bytes)",
          conn.raw_body_length == 15);

    // Simulate keep-alive buffer cleanup as done in onWritable
    size_t consumed = conn.header_end + 4 + conn.raw_body_length;
    conn.read_buffer.erase(0, consumed);

    check("Chunked: leftover buffer starts exactly with second pipelined request",
          conn.read_buffer == second_request);

    // Reset connection state for next request
    conn.headers_complete = false;
    conn.content_length = 0;
    conn.raw_body_length = 0;
    conn.header_end = 0;
    conn.chunked = false;

    RequestReadState next_state = Request::isRequestComplete(conn);
    check("Chunked: second pipelined request completes successfully",
          next_state == RequestReadState::COMPLETE);

    delete conn.settings;
}

static void test_expect_live_socket_continue_and_resubscribe_read() {
    int fds[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) < 0)
        return;

    fcntl(fds[0], F_SETFL, O_NONBLOCK);
    fcntl(fds[1], F_SETFL, O_NONBLOCK);

    bool read_cb_fired = false;
    PollHandler::getInstance().subscribe_read(fds[0],
        [fd = fds[0]]() { PollHandler::getInstance().unsubscribe(fd); },
        [&read_cb_fired]() { read_cb_fired = true; }
    );

    std::string raw =
        "POST /upload HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 10\r\n"
        "Expect: 100-continue\r\n"
        "\r\n";

    Connection conn = createDummyConnection(raw, 1000);
    conn.fd = fds[0];

    RequestReadState state = Request::isRequestComplete(conn);
    check("Expect: 100-continue returns INCOMPLETE", state == RequestReadState::INCOMPLETE);
    check("Expect: sent_100_continue is marked true", conn.sent_100_continue == true);

    // Poll should fire write callback and send 100 Continue
    PollHandler::getInstance().setTimeout(50);
    PollHandler::getInstance().checkFDs();

    // Check data received on other end
    char buf[128];
    ssize_t n = ::read(fds[1], buf, sizeof(buf));
    std::string received(buf, n > 0 ? n : 0);
    check("Expect: 100 Continue response sent to socket", received == "HTTP/1.1 100 Continue\r\n\r\n");

    // Check that fds[0] is now re-subscribed for reading
    auto ev = PollHandler::getInstance().getEventByFD(fds[0]);
    check("Expect: socket event exists in poll", ev != nullptr);
    check("Expect: socket is subscribed for reading", ev && ev->on_readable != nullptr);
    check("Expect: socket is not subscribed for writing anymore", ev && ev->on_writeable == nullptr);

    // Send payload from client end to verify read callback fires
    ::write(fds[1], "0123456789", 10);
    PollHandler::getInstance().checkFDs();
    check("Expect: read callback fired after 100 Continue", read_cb_fired == true);

    PollHandler::getInstance().unsubscribe(fds[0]);
    ::close(fds[0]);
    ::close(fds[1]);
    delete conn.settings;
}

int main() {
    std::cout << "--- Expect & Chunked Keep-Alive Test Suite ---\n" << std::endl;

    test_expect_payload_too_large();
    test_expect_same_time_headers_and_payload();
    test_expect_partial_payload();
    test_expect_invalid_expectation();
    test_expect_live_socket_continue_and_resubscribe_read();
    test_chunked_keep_alive_buffer_cleanup();

    std::cout << "\n----------------------------------------" << std::endl;
    std::cout << "Results: " << g_passed << " passed, " << g_failed << " failed, "
              << (g_passed + g_failed) << " total" << std::endl;

    return g_failed > 0 ? 1 : 0;
}
