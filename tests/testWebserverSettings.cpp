#include "../src/WebserverSettings.hpp"
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

// --------------- listen ---------------

static void test_listen_port_only() {
	WebserverSettings ws = WebserverSettings::fromBlock(
		"listen 8080;\n"
	);
	check("listen port only: 1 listen entry", ws.listen.size() == 1);
	check("listen port only: port is 8080", ws.listen[0].port == 8080);
	check("listen port only: address is 0.0.0.0", ws.listen[0].address == "0.0.0.0");
	check("listen port only: not default", ws.listen[0].is_default == false);
}

static void test_listen_address_port() {
	WebserverSettings ws = WebserverSettings::fromBlock(
		"listen 127.0.0.1:3000;\n"
	);
	check("listen addr:port: port is 3000", ws.listen[0].port == 3000);
	check("listen addr:port: address is 127.0.0.1", ws.listen[0].address == "127.0.0.1");
}

static void test_listen_default_server() {
	WebserverSettings ws = WebserverSettings::fromBlock(
		"listen 80 default_server;\n"
	);
	check("listen default_server: is_default true", ws.listen[0].is_default == true);
	check("listen default_server: port is 80", ws.listen[0].port == 80);
}

static void test_listen_multiple() {
	WebserverSettings ws = WebserverSettings::fromBlock(
		"listen 80;\n"
		"listen 443;\n"
	);
	check("listen multiple: 2 entries", ws.listen.size() == 2);
	check("listen multiple: first port 80", ws.listen[0].port == 80);
	check("listen multiple: second port 443", ws.listen[1].port == 443);
}

// --------------- server_name ---------------

static void test_server_name_single() {
	WebserverSettings ws = WebserverSettings::fromBlock(
		"server_name example.com;\n"
	);
	check("server_name single: 1 name", ws.server_name.size() == 1);
	check("server_name single: is example.com", ws.server_name[0] == "example.com");
}

static void test_server_name_multiple() {
	WebserverSettings ws = WebserverSettings::fromBlock(
		"server_name example.com www.example.com;\n"
	);
	check("server_name multiple: 2 names", ws.server_name.size() == 2);
	check("server_name multiple: first", ws.server_name[0] == "example.com");
	check("server_name multiple: second", ws.server_name[1] == "www.example.com");
}

// --------------- root ---------------

static void test_root() {
	WebserverSettings ws = WebserverSettings::fromBlock(
		"root /var/www;\n"
	);
	check("root: is /var/www", ws.root == "/var/www");
}

// --------------- index ---------------

static void test_index() {
	WebserverSettings ws = WebserverSettings::fromBlock(
		"index index.php;\n"
	);
	check("index: is index.php", ws.index == "index.php");
}

// --------------- dirindex ---------------

static void test_dirindex_on() {
	WebserverSettings ws = WebserverSettings::fromBlock(
		"dirindex on;\n"
	);
	check("dirindex on: true", ws.dirindex == true);
}

static void test_dirindex_off() {
	WebserverSettings ws = WebserverSettings::fromBlock(
		"dirindex off;\n"
	);
	check("dirindex off: false", ws.dirindex == false);
}

static void test_dirindex_true_keyword() {
	WebserverSettings ws = WebserverSettings::fromBlock(
		"dirindex true;\n"
	);
	check("dirindex true: true", ws.dirindex == true);
}

// --------------- location block ---------------

static void test_location_empty() {
	WebserverSettings ws = WebserverSettings::fromBlock(
		"location / {\n"
		"}\n"
	);
	check("location empty: 1 location", ws.locations.size() == 1);
	check("location empty: path is /", ws.locations.find("/") != ws.locations.end());
}

static void test_location_with_root() {
	WebserverSettings ws = WebserverSettings::fromBlock(
		"location /images {\n"
		"root /var/images;\n"
		"}\n"
	);
	check("location root: path exists", ws.locations.find("/images") != ws.locations.end());
	const auto& loc = ws.locations.at("/images");
	check("location root: has value", loc.root.has_value());
	check("location root: is /var/images", loc.root.value() == "/var/images");
}

static void test_location_multiline() {
	WebserverSettings ws = WebserverSettings::fromBlock(
		"location /api {\n"
		"root /var/api;\n"
		"index api.php;\n"
		"dirindex off;\n"
		"}\n"
	);
	check("location multiline: path exists", ws.locations.find("/api") != ws.locations.end());
	const auto& loc = ws.locations.at("/api");
	check("location multiline: root", loc.root.value() == "/var/api");
	check("location multiline: index", loc.index == "api.php");
	check("location multiline: dirindex off", loc.dirindex == false);
}

static void test_location_cgi_extension() {
	WebserverSettings ws = WebserverSettings::fromBlock(
		"location /cgi {\n"
		"cgi_extension .php;\n"
		"}\n"
	);
	const auto& loc = ws.locations.at("/cgi");
	check("location cgi_extension: .php", loc.cgi_extension == ".php");
}

static void test_location_redirect() {
	WebserverSettings ws = WebserverSettings::fromBlock(
		"location /old {\n"
		"redirect /new;\n"
		"}\n"
	);
	const auto& loc = ws.locations.at("/old");
	check("location redirect: /new", loc.redirect == "/new");
}

static void test_location_upload_dir() {
	WebserverSettings ws = WebserverSettings::fromBlock(
		"location /upload {\n"
		"upload_dir /tmp/uploads;\n"
		"}\n"
	);
	const auto& loc = ws.locations.at("/upload");
	check("location upload_dir: /tmp/uploads", loc.upload_dir == "/tmp/uploads");
}

static void test_multiple_locations() {
	WebserverSettings ws = WebserverSettings::fromBlock(
		"location / {\n"
		"root /var/www;\n"
		"}\n"
		"location /api {\n"
		"root /var/api;\n"
		"}\n"
		"location /images {\n"
		"root /var/images;\n"
		"}\n"
	);
	check("multiple locations: 3 total", ws.locations.size() == 3);
	check("multiple locations: / exists", ws.locations.find("/") != ws.locations.end());
	check("multiple locations: /api exists", ws.locations.find("/api") != ws.locations.end());
	check("multiple locations: /images exists", ws.locations.find("/images") != ws.locations.end());
}

// --------------- defaults ---------------

static void test_defaults_empty_block() {
	WebserverSettings ws = WebserverSettings::fromBlock("\n");
	check("defaults: dirindex false", ws.dirindex == false);
	check("defaults: index is index.html", ws.index == "index.html");
	check("defaults: root empty", ws.root.empty());
	check("defaults: no listen entries", ws.listen.empty());
	check("defaults: no server names", ws.server_name.empty());
	check("defaults: no locations", ws.locations.empty());
}

// --------------- combined ---------------

static void test_full_server_block() {
	WebserverSettings ws = WebserverSettings::fromBlock(
		"listen 80 default_server;\n"
		"listen 443;\n"
		"server_name example.com www.example.com;\n"
		"root /var/www;\n"
		"index index.html;\n"
		"dirindex on;\n"
		"location / {\n"
		"root /var/www/html;\n"
		"}\n"
		"location /api {\n"
		"root /var/api;\n"
		"cgi_extension .php;\n"
		"}\n"
	);
	check("full block: listen count 2", ws.listen.size() == 2);
	check("full block: server_name count 2", ws.server_name.size() == 2);
	check("full block: root", ws.root == "/var/www");
	check("full block: index", ws.index == "index.html");
	check("full block: dirindex true", ws.dirindex == true);
	check("full block: locations count 2", ws.locations.size() == 2);
	check("full block: location / exists", ws.locations.find("/") != ws.locations.end());
	check("full block: location /api exists", ws.locations.find("/api") != ws.locations.end());
	check("full block: location / root override", ws.locations.at("/").root.value() == "/var/www/html");
	check("full block: location /api cgi", ws.locations.at("/api").cgi_extension == ".php");
}

int main() {
	std::cout << "--- WebserverSettings Tests ---\n" << std::endl;

	std::cout << "listen:" << std::endl;
	test_listen_port_only();
	test_listen_address_port();
	test_listen_default_server();
	test_listen_multiple();

	std::cout << "\nserver_name:" << std::endl;
	test_server_name_single();
	test_server_name_multiple();

	std::cout << "\nroot:" << std::endl;
	test_root();

	std::cout << "\nindex:" << std::endl;
	test_index();

	std::cout << "\ndirindex:" << std::endl;
	test_dirindex_on();
	test_dirindex_off();
	test_dirindex_true_keyword();

	std::cout << "\nlocation block:" << std::endl;
	test_location_empty();
	test_location_with_root();
	test_location_multiline();
	test_location_cgi_extension();
	test_location_redirect();
	test_location_upload_dir();
	test_multiple_locations();

	std::cout << "\ndefaults:" << std::endl;
	test_defaults_empty_block();

	std::cout << "\ncombined:" << std::endl;
	test_full_server_block();

	std::cout << "\n----------------------------------------" << std::endl;
	std::cout << "Results: " << g_passed << " passed, " << g_failed << " failed, "
	          << (g_passed + g_failed) << " total" << std::endl;
	return g_failed > 0 ? 1 : 0;
}
