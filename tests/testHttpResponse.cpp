#include "../src/HttpResponse.hpp"
#include <iostream>
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

static bool str_contains(const std::string& haystack, const std::string& needle) {
	return haystack.find(needle) != std::string::npos;
}

// --------------- construction / defaults ---------------

static void test_default_constructor() {
	HttpResponse resp;
	check("default: status is 200", resp.getStatus() == 200);
	check("default: body is empty", resp.getBody().empty());
	check("default: keep_alive is false", resp.getKeepAlive() == false);
	check("default: headers are empty", resp.getHeaders().empty());
}

// --------------- setters / getters ---------------

static void test_setStatus() {
	HttpResponse resp;
	resp.setStatus(404);
	check("setStatus(404): getStatus returns 404", resp.getStatus() == 404);
	resp.setStatus(500);
	check("setStatus(500): getStatus returns 500", resp.getStatus() == 500);
}

static void test_setBody() {
	HttpResponse resp;
	resp.setBody("Hello World");
	check("setBody: getBody returns correct string", resp.getBody() == "Hello World");

	HttpResponse resp2;
	resp2.setBody("");
	check("setBody empty: getBody returns empty", resp2.getBody().empty());
}

static void test_setKeepAlive() {
	HttpResponse resp;
	resp.setKeepAlive(true);
	check("setKeepAlive(true): getKeepAlive is true", resp.getKeepAlive() == true);
	resp.setKeepAlive(false);
	check("setKeepAlive(false): getKeepAlive is false", resp.getKeepAlive() == false);
}

// --------------- headers ---------------

static void test_addHeader() {
	HttpResponse resp;
	resp.addHeader("Content-Type", "text/plain");
	const auto& headers = resp.getHeaders();
	check("addHeader: header count is 1", headers.size() == 1);
	check("addHeader: key exists", headers.find("Content-Type") != headers.end());
	check("addHeader: value correct", headers.at("Content-Type") == "text/plain");
}

static void test_addHeader_overwrite() {
	HttpResponse resp;
	resp.addHeader("X-Custom", "first");
	resp.addHeader("X-Custom", "second");
	check("addHeader overwrite: key exists", resp.getHeaders().find("X-Custom") != resp.getHeaders().end());
	check("addHeader overwrite: value is second", resp.getHeaders().at("X-Custom") == "second");
}

static void test_addMultipleHeaders() {
	HttpResponse resp;
	resp.addHeader("Content-Type", "text/html");
	resp.addHeader("Server", "webserv");
	resp.addHeader("X-Frame-Options", "DENY");
	check("multiple headers: count is 3", resp.getHeaders().size() == 3);
}

static void test_removeHeader() {
	HttpResponse resp;
	resp.addHeader("X-Test", "value");
	resp.removeHeader("X-Test");
	check("removeHeader: header removed", resp.getHeaders().find("X-Test") == resp.getHeaders().end());
}

static void test_removeHeader_nonexistent() {
	HttpResponse resp;
	resp.addHeader("X-Test", "value");
	resp.removeHeader("X-DoesNotExist");
	check("removeHeader nonexistent: no crash", resp.getHeaders().size() == 1);
}

// --------------- toString ---------------

static void test_toString_startLine() {
	HttpResponse resp;
	std::string out = resp.toString();
	check("toString startLine: HTTP/1.1 present", str_contains(out, "HTTP/1.1 200 OK\r\n"));
}

static void test_toString_statusChange() {
	HttpResponse resp;
	resp.setStatus(404);
	std::string out = resp.toString();
	check("toString status 404: in output", str_contains(out, "HTTP/1.1 404 Not Found\r\n"));

	resp.setStatus(500);
	out = resp.toString();
	check("toString status 500: in output", str_contains(out, "HTTP/1.1 500 Internal Server Error\r\n"));
}

static void test_toString_connectionClose() {
	HttpResponse resp;
	resp.setKeepAlive(false);
	std::string out = resp.toString();
	check("toString close: Connection: close", str_contains(out, "Connection: close\r\n"));
}

static void test_toString_connectionKeepAlive() {
	HttpResponse resp;
	resp.setKeepAlive(true);
	std::string out = resp.toString();
	check("toString keep-alive: Connection: keep-alive", str_contains(out, "Connection: keep-alive\r\n"));
}

static void test_toString_contentLength() {
	HttpResponse resp;
	resp.setBody("12345");
	std::string out = resp.toString();
	check("toString Content-Length: 5", str_contains(out, "Content-Length: 5\r\n"));
}

static void test_toString_contentTypeDefault() {
	HttpResponse resp;
	resp.setBody("<html></html>");
	std::string out = resp.toString();
	check("toString default Content-Type: text/html", str_contains(out, "Content-Type: text/html\r\n"));
}

static void test_toString_contentTypeCustom() {
	HttpResponse resp;
	resp.addHeader("Content-Type", "application/json");
	resp.setBody("{}");
	std::string out = resp.toString();
	check("toString custom Content-Type: application/json", str_contains(out, "Content-Type: application/json\r\n"));
}

static void test_toString_bodyInOutput() {
	HttpResponse resp;
	resp.setBody("<h1>Test</h1>");
	std::string out = resp.toString();
	check("toString body: present", str_contains(out, "<h1>Test</h1>"));
	check("toString body: after blank line", (str_contains(out, "\r\n\r\n<h1>Test</h1>") || (str_contains(out, "\r\n\r\n") && out.find("<h1>Test</h1>") > out.find("\r\n\r\n"))));
}

static void test_toString_customHeaderInOutput() {
	HttpResponse resp;
	resp.addHeader("X-Powered-By", "webserv");
	std::string out = resp.toString();
	check("toString custom header: present", str_contains(out, "X-Powered-By: webserv\r\n"));
}

static void test_toString_emptyBody() {
	HttpResponse resp;
	resp.setBody("");
	std::string out = resp.toString();
	check("toString empty body: no body output", out.find("\r\n\r\n") == out.size() - 4);
}

static void test_toString_separator() {
	HttpResponse resp;
	resp.setBody("test");
	std::string out = resp.toString();
	check("toString separator: \\r\\n\\r\\n before body",
		str_contains(out, "\r\n\r\ntest"));
}

// --------------- error() ---------------

static void test_error_factory_200() {
	HttpResponse resp = HttpResponse::error(200);
	std::string out = resp.toString();
	check("error(200): status 200", resp.getStatus() == 200);
	check("error(200): body contains 200", str_contains(resp.getBody(), "200"));
	check("error(200): Content-Type is text/html", str_contains(out, "Content-Type: text/html\r\n"));
}

static void test_error_factory_404() {
	HttpResponse resp = HttpResponse::error(404);
	check("error(404): status 404", resp.getStatus() == 404);
	check("error(404): body contains 404", str_contains(resp.getBody(), "404"));
	check("error(404): body contains Not Found", str_contains(resp.getBody(), "Not Found"));
}

static void test_error_factory_500() {
	HttpResponse resp = HttpResponse::error(500);
	check("error(500): status 500", resp.getStatus() == 500);
	check("error(500): body contains 500", str_contains(resp.getBody(), "500"));
}

static void test_error_factory_501() {
	HttpResponse resp = HttpResponse::error(501);
	check("error(501): status 501", resp.getStatus() == 501);
	check("error(501): keep_alive false", resp.getKeepAlive() == false);
}

static void test_error_factory_301() {
	HttpResponse resp = HttpResponse::error(301);
	check("error(301): status 301", resp.getStatus() == 301);
	check("error(301): body contains 301", str_contains(resp.getBody(), "301"));
}

int main() {
	std::cout << "--- HttpResponse Tests ---\n" << std::endl;

	std::cout << "Construction & Defaults:" << std::endl;
	test_default_constructor();

	std::cout << "\nSetters & Getters:" << std::endl;
	test_setStatus();
	test_setBody();
	test_setKeepAlive();

	std::cout << "\nHeaders:" << std::endl;
	test_addHeader();
	test_addHeader_overwrite();
	test_addMultipleHeaders();
	test_removeHeader();
	test_removeHeader_nonexistent();

	std::cout << "\ntoString:" << std::endl;
	test_toString_startLine();
	test_toString_statusChange();
	test_toString_connectionClose();
	test_toString_connectionKeepAlive();
	test_toString_contentLength();
	test_toString_contentTypeDefault();
	test_toString_contentTypeCustom();
	test_toString_bodyInOutput();
	test_toString_customHeaderInOutput();
	test_toString_emptyBody();
	test_toString_separator();

	std::cout << "\nerror() Factory:" << std::endl;
	test_error_factory_200();
	test_error_factory_404();
	test_error_factory_500();
	test_error_factory_501();
	test_error_factory_301();

	std::cout << "\n----------------------------------------" << std::endl;
	std::cout << "Results: " << g_passed << " passed, " << g_failed << " failed, "
	          << (g_passed + g_failed) << " total" << std::endl;
	return g_failed > 0 ? 1 : 0;
}
