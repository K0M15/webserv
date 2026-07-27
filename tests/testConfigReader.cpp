#include "ConfigReader.h"
#include "WebserverSettings.hpp"
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

// --------------- single server config ---------------

static void test_single_server() {
    ConfigReader reader("tests/sample_cfg/single_url.config");
    check("single: 1 server block", reader.data.size() == 1u);

    const auto& ws = reader.data.at("single.example.com");
    check("single: 1 listen entry", ws.listen.size() == 1u);
    check("single: listen port 4000", ws.listen[0].port == 4000);
    check("single: root is /", ws.root == "/");
    check("single: server_name", ws.server_name == "single.example.com");
    check("single: 1 location", ws.locations.size() == 1u);
    check("single: location / exists", ws.locations.find("/") != ws.locations.end());
    check("single: location / root", ws.locations.at("/").root.value_or("") == "/var/www/");
}

// --------------- multi-server config ---------------

static void test_multi_server() {
    ConfigReader reader("tests/sample_cfg/multi_server.config");
    check("multi: 2 server entries", reader.data.size() == 2u);

    // Server 1: example.com on port 4000
    check("multi: example.com exists",
        reader.data.find("example.com") != reader.data.end());
    const auto& s1 = reader.data.at("example.com");
    check("multi: s1 port 4000", s1.listen[0].port == 4000);
    check("multi: s1 root", s1.root == "/var/www/site1");
    check("multi: s1 index", s1.index == "index.html");
    check("multi: s1 server_name", s1.server_name == "example.com");
    check("multi: s1 missing_content_type REJECT",
        s1.missing_content_type_policy == MissingContentTypePolicy::REJECT);
    check("multi: s1 location / exists",
        s1.locations.find("/") != s1.locations.end());
    check("multi: s1 no /upload",
        s1.locations.find("/upload") == s1.locations.end());

    // Server 2: api.example.com on port 5000
    check("multi: api.example.com exists",
        reader.data.find("api.example.com") != reader.data.end());
    const auto& s2 = reader.data.at("api.example.com");
    check("multi: s2 port 5000", s2.listen[0].port == 5000);
    check("multi: s2 root", s2.root == "/var/www/site2");
    check("multi: s2 index", s2.index == "index.php");
    check("multi: s2 server_name", s2.server_name == "api.example.com");
    check("multi: s2 missing_content_type DEFAULT",
        s2.missing_content_type_policy == MissingContentTypePolicy::DEFAULT);
    check("multi: s2 missing_content_type default type",
        s2.missing_content_type_default == "application/json");
    check("multi: s2 location /upload exists",
        s2.locations.find("/upload") != s2.locations.end());

    const auto& upload = s2.locations.at("/upload");
    check("multi: upload_dir", upload.upload_dir == "/tmp/uploads");
    check("multi: upload missing_content_type overridden",
        upload.missing_content_type_policy.has_value());
    check("multi: upload missing_content_type DEFAULT",
        upload.missing_content_type_policy.value() == MissingContentTypePolicy::DEFAULT);
    check("multi: upload default type",
        upload.missing_content_type_default.value_or("") == "application/octet-stream");
    check("multi: s2 location / exists",
        s2.locations.find("/") != s2.locations.end());
}

// --------------- virtual host config ---------------

static void test_virtual_host() {
    ConfigReader reader("tests/sample_cfg/virtual_host.config");
    check("vh: 2 server entries", reader.data.size() == 2u);

    check("vh: example.com exists",
        reader.data.find("example.com") != reader.data.end());
    const auto& s1 = reader.data.at("example.com");
    check("vh: s1 port 8080", s1.listen[0].port == 8080);
    check("vh: s1 root", s1.root == "/var/www/site1");
    check("vh: s1 server_name", s1.server_name == "example.com");

    check("vh: api.example.com exists",
        reader.data.find("api.example.com") != reader.data.end());
    const auto& s2 = reader.data.at("api.example.com");
    check("vh: s2 port 8080", s2.listen[0].port == 8080);
    check("vh: s2 root", s2.root == "/var/www/site2");
    check("vh: s2 server_name", s2.server_name == "api.example.com");
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    std::cout << "--- ConfigReader Tests ---\n" << std::endl;

    std::cout << "Single Server:" << std::endl;
    test_single_server();
    std::cout << std::endl;

    std::cout << "Multi Server:" << std::endl;
    test_multi_server();
    std::cout << std::endl;

    std::cout << "Virtual Host:" << std::endl;
    test_virtual_host();

    std::cout << "\n----------------------------------------" << std::endl;
    std::cout << "Results: " << g_passed << " passed, " << g_failed << " failed, "
              << (g_passed + g_failed) << " total" << std::endl;
    return g_failed > 0 ? 1 : 0;
}
