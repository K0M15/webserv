#include "ConnectionManager.hpp"
#include "WebserverSettings.hpp"
#include "PollHandler.hpp"
#include <arpa/inet.h>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <unistd.h>

static int g_passed = 0;
static int g_failed = 0;

static void pass(const char* name) {
    std::cout << "[SUCCESS] " << name << std::endl;
    ++g_passed;
}

static void fail(const char* name, const std::string& msg) {
    std::cout << "[FAILURE] " << name;
    if (!msg.empty()) std::cout << " - " << msg;
    std::cout << std::endl;
    ++g_failed;
}

static void check(const char* name, bool condition, const std::string& fail_msg = "") {
    if (condition)
        pass(name);
    else
        fail(name, fail_msg);
}

// --- tiny raw-HTTP helpers ---------------------------------------------

static std::string statusLine(const std::string& raw) {
    return raw.substr(0, raw.find("\r\n"));
}

static std::string headerValue(const std::string& raw, const std::string& name) {
    std::string lower_raw = raw;
    std::string needle = "\n" + name + ":";
    std::string lower_needle = needle;
    for (auto& c : lower_raw) c = std::tolower(static_cast<unsigned char>(c));
    for (auto& c : lower_needle) c = std::tolower(static_cast<unsigned char>(c));
    size_t pos = lower_raw.find(lower_needle);
    if (pos == std::string::npos) return "";
    pos += lower_needle.size();
    size_t end = raw.find("\r\n", pos);
    std::string value = raw.substr(pos, end - pos);
    size_t first = value.find_first_not_of(' ');
    return first == std::string::npos ? "" : value.substr(first);
}

static std::string bodyOf(const std::string& raw) {
    size_t pos = raw.find("\r\n\r\n");
    return pos == std::string::npos ? "" : raw.substr(pos + 4);
}

// --- minimal one-shot server harness ------------------------------------
// Sets up a real listening socket backed by a ConnectionManager, so requests
// go through the exact same handleRequest()/handleHead()/sendResponse() path
// the real server uses - not a mock.

struct TestServer {
    ConnectionManager manager;
    WebserverSettings settings;
    int listen_fd;
    uint16_t port;

    explicit TestServer(const std::vector<Method>& allowed_methods = {}) {
        settings.root = "tests/sample_www";
        settings.server_name.push_back("localhost");
        settings.methods = allowed_methods;

        listen_fd = socket(AF_INET, SOCK_STREAM, 0);
        int opt = 1;
        setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        addr.sin_port = 0; // let the OS pick a free port

        bind(listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
        listen(listen_fd, 16);

        socklen_t len = sizeof(addr);
        getsockname(listen_fd, reinterpret_cast<sockaddr*>(&addr), &len);
        port = ntohs(addr.sin_port);

        fcntl(listen_fd, F_SETFL, O_NONBLOCK);

        settings.listen.push_back({"127.0.0.1", port, false});

        PollHandler::getInstance().subscribe_read(listen_fd, nullptr,
            [this]() {
                std::vector<const WebserverSettings*> candidates{&settings};
                manager.acceptConnection(listen_fd, candidates);
            });
    }

    ~TestServer() {
        PollHandler::getInstance().unsubscribe(listen_fd);
        close(listen_fd);
    }

    // Connects, sends a raw HTTP request, pumps the event loop until a full
    // response has been received (or a bounded number of iterations pass).
    std::string request(const std::string& raw_request) {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        addr.sin_port = htons(port);

        if (connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
            close(fd);
            return "";
        }
        send(fd, raw_request.data(), raw_request.size(), 0);

        PollHandler& poll = PollHandler::getInstance();
        poll.setTimeout(20);

        std::string response;
        char buf[4096];
        for (int i = 0; i < 100; ++i) {
            poll.checkFDs();
            ssize_t n;
            while ((n = recv(fd, buf, sizeof(buf), MSG_DONTWAIT)) > 0)
                response.append(buf, n);
            if (response.find("\r\n\r\n") != std::string::npos) {
                // a couple more spins to pick up any trailing body bytes
                for (int j = 0; j < 5; ++j) {
                    poll.checkFDs();
                    while ((n = recv(fd, buf, sizeof(buf), MSG_DONTWAIT)) > 0)
                        response.append(buf, n);
                }
                break;
            }
        }

        poll.setTimeout(3000);
        close(fd);
        return response;
    }
};

int main() {
    // --------------- HEAD mirrors GET's headers but sends no body ---------------
    {
        TestServer server; // unrestricted methods -> isolates the body-stripping behavior
        std::string get = server.request(
            "GET /hello.txt HTTP/1.1\r\nHost: localhost\r\n\r\n");
        std::string head = server.request(
            "HEAD /hello.txt HTTP/1.1\r\nHost: localhost\r\n\r\n");

        check("GET /hello.txt: 200 OK", statusLine(get) == "HTTP/1.1 200 OK", statusLine(get));
        check("HEAD /hello.txt: same status line as GET", statusLine(head) == statusLine(get), statusLine(head));
        check("HEAD /hello.txt: same Content-Type as GET",
            headerValue(head, "Content-Type") == headerValue(get, "Content-Type"));
        check("HEAD /hello.txt: same Content-Length as GET",
            !headerValue(head, "Content-Length").empty() &&
            headerValue(head, "Content-Length") == headerValue(get, "Content-Length"));
        check("GET /hello.txt: body length matches Content-Length",
            std::to_string(bodyOf(get).size()) == headerValue(get, "Content-Length"));
        check("HEAD /hello.txt: body is empty", bodyOf(head).empty(),
            "body was " + std::to_string(bodyOf(head).size()) + " bytes");
    }

    // --------------- HEAD on a missing file still gets no body ---------------
    {
        TestServer server;
        std::string get = server.request(
            "GET /does-not-exist.txt HTTP/1.1\r\nHost: localhost\r\n\r\n");
        std::string head = server.request(
            "HEAD /does-not-exist.txt HTTP/1.1\r\nHost: localhost\r\n\r\n");

        check("GET /does-not-exist.txt: 404", statusLine(get) == "HTTP/1.1 404 Not Found", statusLine(get));
        check("HEAD /does-not-exist.txt: 404", statusLine(head) == statusLine(get), statusLine(head));
        check("HEAD /does-not-exist.txt: same Content-Length as GET's error body",
            !headerValue(get, "Content-Length").empty() &&
            headerValue(head, "Content-Length") == headerValue(get, "Content-Length"));
        check("HEAD /does-not-exist.txt: body is empty", bodyOf(head).empty());
    }

    // --------------- HEAD must be allowed wherever GET is allowed ---------------
    // 'methods' is restricted to GET/POST only (no explicit 'head'), which is a
    // realistic config (see tests/sample_cfg/valid_full_features.conf). Per
    // RFC 7231 4.3.2, a server that supports GET for a resource MUST support
    // HEAD for it too - so this must NOT come back as 405.
    {
        TestServer server({GET, POST});
        std::string get = server.request(
            "GET /hello.txt HTTP/1.1\r\nHost: localhost\r\n\r\n");
        std::string head = server.request(
            "HEAD /hello.txt HTTP/1.1\r\nHost: localhost\r\n\r\n");

        check("restricted methods: GET /hello.txt still 200",
            statusLine(get) == "HTTP/1.1 200 OK", statusLine(get));
        check("restricted methods (GET,POST only): HEAD /hello.txt is NOT 405",
            statusLine(head) != "HTTP/1.1 405 Method Not Allowed", statusLine(head));
        check("restricted methods (GET,POST only): HEAD /hello.txt is 200",
            statusLine(head) == "HTTP/1.1 200 OK", statusLine(head));
    }

    std::cout << std::endl << g_passed << " passed, " << g_failed << " failed" << std::endl;
    return g_failed == 0 ? 0 : 1;
}
