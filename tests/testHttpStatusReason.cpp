#include "../src/HttpStatusReason.hpp"
#include <iostream>

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

// --------------- 1xx ---------------

static void test_1xx() {
	check("100 Continue", std::string(HttpStatusReason::reason(100)) == "Continue");
	check("101 Switching Protocols", std::string(HttpStatusReason::reason(101)) == "Switching Protocols");
}

// --------------- 2xx ---------------

static void test_2xx() {
	check("200 OK", std::string(HttpStatusReason::reason(200)) == "OK");
	check("201 Created", std::string(HttpStatusReason::reason(201)) == "Created");
	check("202 Accepted", std::string(HttpStatusReason::reason(202)) == "Accepted");
	check("204 No Content", std::string(HttpStatusReason::reason(204)) == "No Content");
	check("206 Partial Content", std::string(HttpStatusReason::reason(206)) == "Partial Content");
}

// --------------- 3xx ---------------

static void test_3xx() {
	check("301 Moved Permanently", std::string(HttpStatusReason::reason(301)) == "Moved Permanently");
	check("302 Found", std::string(HttpStatusReason::reason(302)) == "Found");
	check("304 Not Modified", std::string(HttpStatusReason::reason(304)) == "Not Modified");
	check("307 Temporary Redirect", std::string(HttpStatusReason::reason(307)) == "Temporary Redirect");
}

// --------------- 4xx ---------------

static void test_4xx() {
	check("400 Bad Request", std::string(HttpStatusReason::reason(400)) == "Bad Request");
	check("401 Unauthorized", std::string(HttpStatusReason::reason(401)) == "Unauthorized");
	check("403 Forbidden", std::string(HttpStatusReason::reason(403)) == "Forbidden");
	check("404 Not Found", std::string(HttpStatusReason::reason(404)) == "Not Found");
	check("405 Method Not Allowed", std::string(HttpStatusReason::reason(405)) == "Method Not Allowed");
	check("408 Request Timeout", std::string(HttpStatusReason::reason(408)) == "Request Timeout");
	check("409 Conflict", std::string(HttpStatusReason::reason(409)) == "Conflict");
	check("410 Gone", std::string(HttpStatusReason::reason(410)) == "Gone");
	check("411 Length Required", std::string(HttpStatusReason::reason(411)) == "Length Required");
	check("413 Payload Too Large", std::string(HttpStatusReason::reason(413)) == "Payload Too Large");
	check("414 URI Too Long", std::string(HttpStatusReason::reason(414)) == "URI Too Long");
	check("415 Unsupported Media Type", std::string(HttpStatusReason::reason(415)) == "Unsupported Media Type");
	check("417 Expectation Failed", std::string(HttpStatusReason::reason(417)) == "Expectation Failed");
	check("426 Upgrade Required", std::string(HttpStatusReason::reason(426)) == "Upgrade Required");
	check("429 Too Many Requests", std::string(HttpStatusReason::reason(429)) == "Too Many Requests");
}

// --------------- 5xx ---------------

static void test_5xx() {
	check("500 Internal Server Error", std::string(HttpStatusReason::reason(500)) == "Internal Server Error");
	check("501 Not Implemented", std::string(HttpStatusReason::reason(501)) == "Not Implemented");
	check("502 Bad Gateway", std::string(HttpStatusReason::reason(502)) == "Bad Gateway");
	check("503 Service Unavailable", std::string(HttpStatusReason::reason(503)) == "Service Unavailable");
	check("504 Gateway Timeout", std::string(HttpStatusReason::reason(504)) == "Gateway Timeout");
	check("505 HTTP Version Not Supported", std::string(HttpStatusReason::reason(505)) == "HTTP Version Not Supported");
}

// --------------- edge cases ---------------

static void test_unknown() {
	const char* r = HttpStatusReason::reason(999);
	check("999: returns non-null string", r != nullptr);
	check("999: returns 'Unknown'", std::string(r) == "Unknown");

	r = HttpStatusReason::reason(0);
	check("0: returns non-null string", r != nullptr);
	check("0: returns 'Unknown'", std::string(r) == "Unknown");
}

static void test_enum_vs_unsigned() {
	check("enum 200 == unsigned 200",
		std::string(HttpStatusReason::reason(HttpStatusReason::Code::OK))
		== std::string(HttpStatusReason::reason(200u)));
}

// --------------- pointer stability ---------------

static void test_reason_returns_const() {
	const char* r1 = HttpStatusReason::reason(200);
	const char* r2 = HttpStatusReason::reason(200);
	check("same pointer for same code", r1 == r2);
}

int main() {
	std::cout << "--- HttpStatusReason Tests ---\n" << std::endl;

	test_1xx();
	std::cout << std::endl;
	test_2xx();
	std::cout << std::endl;
	test_3xx();
	std::cout << std::endl;
	test_4xx();
	std::cout << std::endl;
	test_5xx();
	std::cout << std::endl;

	std::cout << "Edge Cases:" << std::endl;
	test_unknown();
	test_enum_vs_unsigned();

	std::cout << "\nPointer Stability:" << std::endl;
	test_reason_returns_const();

	std::cout << "\n----------------------------------------" << std::endl;
	std::cout << "Results: " << g_passed << " passed, " << g_failed << " failed, "
	          << (g_passed + g_failed) << " total" << std::endl;
	return g_failed > 0 ? 1 : 0;
}
