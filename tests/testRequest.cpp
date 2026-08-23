#include "Request.hpp"
#include <fstream>
#include <iostream>
#include <sstream>

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

// --------------- Cookie Implementation Tests ---------------

static void test_cookie_multiple() {
    std::string raw =
        "GET /index.html HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Cookie: session_id=8675309; user=admin; theme=dark\r\n"
        "\r\n";
    Request req = Request::fromString(raw);
    check("multiple cookies: session_id", req.getCookie("session_id") == "8675309");
    check("multiple cookies: user", req.getCookie("user") == "admin");
    check("multiple cookies: theme", req.getCookie("theme") == "dark");
    check("multiple cookies: non-existent", req.getCookie("missing") == "");
}

static void test_cookie_whitespace_variants() {
    std::string raw =
        "GET /index.html HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Cookie:   session_id=xyz789 ;   user=john;  theme=light\r\n"
        "\r\n";
    Request req = Request::fromString(raw);
    check("whitespace variants: session_id with leading spaces", req.getCookie("session_id") == "xyz789 ");
    check("whitespace variants: user with leading space after semicolon", req.getCookie("user") == "john");
    check("whitespace variants: theme with spaces", req.getCookie("theme") == "light");
}

static void test_cookie_missing_header() {
    std::string raw =
        "GET /index.html HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "\r\n";
    Request req = Request::fromString(raw);
    check("missing header: getCookie returns empty string", req.getCookie("session_id") == "");
}

static void test_cookie_empty_value() {
    std::string raw =
        "GET /index.html HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Cookie: empty_val=; session_id=456\r\n"
        "\r\n";
    Request req = Request::fromString(raw);
    check("empty value: empty_val returns empty string", req.getCookie("empty_val") == "");
    check("empty value: session_id parsed after empty cookie", req.getCookie("session_id") == "456");
}

static void test_cookie_name_only_pair() {
    std::string raw =
        "GET /index.html HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Cookie: flag; session_id=789; logged_in\r\n"
        "\r\n";
    Request req = Request::fromString(raw);
    check("name-only pair: flag returns empty string", req.getCookie("flag") == "");
    check("name-only pair: session_id parsed after name-only pair", req.getCookie("session_id") == "789");
    check("name-only pair: logged_in returns empty string", req.getCookie("logged_in") == "");
}

static void test_request_file(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << filepath << std::endl;
        ++g_failed;
        return;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string rawRequest = buffer.str();

    try {
        Request req = Request::fromString(rawRequest);

        std::cout << "\n--- Request File Validation (" << filepath << ") ---" << std::endl;
        std::cout << "Method:  " << req.getMethod() << std::endl;
        std::cout << "URL:     " << req.getURL().str() << std::endl;
        std::cout << "Version: " << req.getVersion() << std::endl;
        
        std::cout << "Headers:" << std::endl;
        const std::map<std::string, std::string>& headers = req.getHeaders();
        for (std::map<std::string, std::string>::const_iterator it = headers.begin(); it != headers.end(); ++it) {
            std::cout << "  " << it->first << ": " << it->second << std::endl;
        }

        std::cout << "Body:" << std::endl;
        std::cout << req.getBody() << std::endl;
        std::cout << "--------------------------" << std::endl;
        pass("request file parsing");
    } catch (const std::exception& e) {
        fail("request file parsing", e.what());
    }
}

int main(int argc, char* argv[])
{
    std::cout << "--- Request & Cookie Tests ---\n" << std::endl;

    std::cout << "Cookie Tests:" << std::endl;
    test_cookie_multiple();
    test_cookie_whitespace_variants();
    test_cookie_missing_header();
    test_cookie_empty_value();
    test_cookie_name_only_pair();

    if (argc >= 2) {
        test_request_file(argv[1]);
    }

    std::cout << "\n----------------------------------------" << std::endl;
    std::cout << "Results: " << g_passed << " passed, " << g_failed << " failed, "
              << (g_passed + g_failed) << " total" << std::endl;

    return g_failed > 0 ? 1 : 0;
}