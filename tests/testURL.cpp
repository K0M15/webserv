#include "../src/URL.hpp"
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

// --------------- default construction ---------------

static void test_default_construction() {
    URL url;
    check("default-constructed str() is empty", url.str() == "", "expected empty string");
    check("default-constructed getRawQuery is empty", url.getRawQuery() == "", "expected empty query");
    check("default-constructed getQuery is empty", url.getQuery().empty(), "expected empty map");
}

// --------------- valid URLs ---------------

static void test_valid_urls() {
    struct { const char* url; const char* label; } cases[] = {
        {"http://google.com",               "http://google.com"},
        {"https://www.google.com",          "https://www.google.com"},
        {"ftp://example.com/file.txt",      "ftp://example.com/file.txt"},
        {"http://localhost:8080",           "http://localhost:8080"},
        {"https://127.0.0.1/index.html",    "https://127.0.0.1/index.html"},
        {"http://a.b",                      "http://a.b"},
        {"HTTP://UPPERCASE.COM",            "HTTP scheme uppercase"},
        {"/",                               "root path"},
        {"/index.html",                     "simple path"},
        {"/path/to/resource?query=1",       "path with query"},
        {"/a/b/c/d/e/f",                    "deep path"},
        {"*",                               "asterisk"},
        {"/path-with_underscores",          "path with underscores"},
        {"/path-with-dashes",               "path with dashes"},
        {"/path/with/numbers/123",          "path with numbers"},
        {"/path%20with%20encoding",         "path with percent-encoding"},
    };

    for (const auto& c : cases) {
        try {
            URL url(c.url);
            check(c.label, true, "unexpected exception");
        } catch (const HttpServerException& e) {
            check(c.label, false, e.what());
        }
    }
}

// --------------- invalid URLs ---------------

static void test_invalid_urls() {
    struct { const char* url; const char* label; } cases[] = {
        {"htt://google.com",                "bad scheme"},
        {"http:/google.com",                "single slash"},
        {"http:// google.com",              "space in URL"},
        {"google.com",                      "no scheme"},
        {"http://",                         "empty host"},
        {"http://.com",                     "empty hostname component"},
        {"http:///path",                    "triple slash"},
        {"https://???",                     "invalid chars after scheme"},
        {"",                                "empty string"},
        {"relative/path",                   "relative path (no leading slash)"},
        {"/path/../file",                   "parent directory traversal"},
        {"/path/./file",                    "current directory component"},
        {"/path/../",                       "parent traversal ending in slash"},
        {"/path/./",                        "current dir ending in slash"},
    };

    for (const auto& c : cases) {
        try {
            URL url(c.url);
            check(c.label, false, "should have been rejected");
        } catch (const HttpServerException& e) {
            check(c.label, true, e.what());
        }
    }
}

// --------------- str() ---------------

static void test_str() {
    URL url1("http://example.com");
    check("str() returns original", url1.str() == "http://example.com");

    URL url2("/path/to/file");
    check("str() returns path", url2.str() == "/path/to/file");

    URL url3("*");
    check("str() returns asterisk", url3.str() == "*");
}

// --------------- setURL() ---------------

static void test_setURL() {
    URL url;
    url.setURL("http://example.com");
    check("setURL valid absolute", url.str() == "http://example.com");

    url.setURL("/new/path");
    check("setURL valid origin-form", url.str() == "/new/path");

    url.setURL("*");
    check("setURL valid asterisk", url.str() == "*");

    bool threw = false;
    try {
        url.setURL("invalid url");
    } catch (const HttpServerException&) {
        threw = true;
    }
    check("setURL rejects invalid", threw, "expected exception");

    check("value unchanged after failed setURL", url.str() == "*", "url was modified");
}

// --------------- getRawQuery() ---------------

static void test_getRawQuery() {
    URL no_query("/path");
    check("no query returns empty", no_query.getRawQuery() == "");

    URL simple("/path?foo=bar");
    check("simple query", simple.getRawQuery() == "foo=bar");

    URL empty_value("/path?foo=");
    check("empty value", empty_value.getRawQuery() == "foo=");

    URL no_value("/path?foo");
    check("no value", no_value.getRawQuery() == "foo");

    URL multiple("/path?a=1&b=2&c=3");
    check("multiple params", multiple.getRawQuery() == "a=1&b=2&c=3");

    URL empty_query("/path?");
    check("empty query string", empty_query.getRawQuery() == "");

    URL only_query_char("/path?");
    check("only question mark", only_query_char.getRawQuery() == "");
}

// --------------- getQuery() ---------------

static void test_getQuery() {
    URL u1("/search?q=hello");
    auto m1 = u1.getQuery();
    check("single param size", m1.size() == 1u);
    check("single param key", m1.count("q") == 1u);
    check("single param value", m1["q"] == "hello");

    URL u2("/search?q=hello&lang=en&page=2");
    auto m2 = u2.getQuery();
    check("multi param size", m2.size() == 3u);
    check("multi param q", m2["q"] == "hello");
    check("multi param lang", m2["lang"] == "en");
    check("multi param page", m2["page"] == "2");

    URL u3("/path?key_only");
    auto m3 = u3.getQuery();
    check("no value size", m3.size() == 1u);
    check("no value key", m3.count("key_only") == 1u);
    check("no value value", m3["key_only"] == "");

    URL u4("/path?empty=");
    auto m4 = u4.getQuery();
    check("empty value size", m4.size() == 1u);
    check("empty value key", m4["empty"] == "");

    URL u5("/path");
    auto m5 = u5.getQuery();
    check("no query returns empty map", m5.empty());
}

// --------------- operator>> ---------------

static void test_stream_extraction() {
    std::istringstream ss("http://example.com");
    URL url;
    ss >> url;
    check("stream extraction", url.str() == "http://example.com");

    std::istringstream bad("not a valid url");
    URL url2;
    bool threw = false;
    try {
        bad >> url2;
    } catch (const HttpServerException&) {
        threw = true;
    }
    check("stream extraction rejects invalid", threw, "expected exception");
}

// --------------- edge: getQuery with duplicate keys ---------------

static void test_getQuery_duplicate_keys() {
    URL url("/path?key=a&key=b");
    auto m = url.getQuery();
    check("duplicate key last value wins", m["key"] == "b");
}

// --------------- edge: getQuery with trailing ampersand ---------------

static void test_getQuery_trailing_ampersand() {
    URL url("/path?a=1&b=2&");
    auto m = url.getQuery();
    check("trailing & size", m.size() == 2u, "expected 2 params");
    check("trailing & a", m["a"] == "1");
    check("trailing & b", m["b"] == "2");
}

// --------------- edge: getQuery with leading ampersand ---------------

static void test_getQuery_leading_ampersand() {
    URL url("/path?&a=1");
    auto m = url.getQuery();
    check("leading & size", m.size() == 1u, "expected 1 param");
    check("leading & a", m["a"] == "1");
}

// --------------- copy / assign (implicit) ---------------

static void test_copy_and_assign() {
    URL original("/original");
    URL copied(original);
    check("copy construction", copied.str() == "/original");

    URL assigned("/assigned");
    assigned = original;
    check("copy assignment", assigned.str() == "/original");
}

int main() {
    std::cout << "--- URL Tests ---\n" << std::endl;

    std::cout << "Default Construction:" << std::endl;
    test_default_construction();
    std::cout << std::endl;

    std::cout << "Valid URLs:" << std::endl;
    test_valid_urls();
    std::cout << std::endl;

    std::cout << "Invalid URLs:" << std::endl;
    test_invalid_urls();
    std::cout << std::endl;

    std::cout << "str():" << std::endl;
    test_str();
    std::cout << std::endl;

    std::cout << "setURL():" << std::endl;
    test_setURL();
    std::cout << std::endl;

    std::cout << "getRawQuery():" << std::endl;
    test_getRawQuery();
    std::cout << std::endl;

    std::cout << "getQuery():" << std::endl;
    test_getQuery();
    std::cout << std::endl;

    std::cout << "getQuery() Edge Cases:" << std::endl;
    test_getQuery_duplicate_keys();
    test_getQuery_trailing_ampersand();
    test_getQuery_leading_ampersand();
    std::cout << std::endl;

    std::cout << "Copy / Assign:" << std::endl;
    test_copy_and_assign();
    std::cout << std::endl;

    std::cout << "Stream Extraction:" << std::endl;
    test_stream_extraction();

    std::cout << "\n----------------------------------------" << std::endl;
    std::cout << "Results: " << g_passed << " passed, " << g_failed << " failed, "
              << (g_passed + g_failed) << " total" << std::endl;
    return g_failed > 0 ? 1 : 0;
}
