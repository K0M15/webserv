#include "ConfigReader.h"
#include "WebserverSettings.hpp"
#include <iostream>
#include <algorithm>

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

static void check_throws(const char* name, const std::string& path) {
    bool threw = false;
    try {
        ConfigReader reader(path);
        (void)reader;
    } catch (const std::exception&) {
        threw = true;
    }
    check(name, threw, "expected exception");
}

static bool hasName(const WebserverSettings& ws, const std::string& name) {
    return std::find(ws.server_name.begin(), ws.server_name.end(), name)
           != ws.server_name.end();
}

// --------------- single server config ---------------

static void test_single_server() {
    ConfigReader reader("tests/sample_cfg/single_url.config");
    check("single: 1 server block", reader.getAllServers().size() == 1u);

    const WebserverSettings* ws = reader.getAllServers()[0].get();
    check("single: 1 listen entry", ws->listen.size() == 1u);
    check("single: listen port 4000", ws->listen[0].port == 4000);
    check("single: root is /", ws->root == "/");
    check("single: 1 location", ws->locations.size() == 1u);
    check("single: location / exists", ws->locations.find("/") != ws->locations.end());
    check("single: location / root", ws->locations.at("/").root == "/var/www/");
}

// --------------- multi-server config ---------------

static void test_multi_server() {
    ConfigReader reader("tests/sample_cfg/multi_server.config");
    check("multi: 2 server blocks kept", reader.getAllServers().size() == 2u);

    const WebserverSettings* site1 = reader.getAllServers()[0].get();
    check("multi: block 1 listen port 4000", site1->listen[0].port == 4000);
    check("multi: block 1 root /var/www/site1", site1->root == "/var/www/site1");
    check("multi: block 1 index index.html", site1->index == "index.html");
    check("multi: block 1 server_name example.com", hasName(*site1, "example.com"));
    check("multi: block 1 missing_content_type REJECT",
        site1->missing_content_type_policy == MissingContentTypePolicy::REJECT);
    check("multi: block 1 location / root",
        site1->locations.at("/").root == "/var/www/site1");

    const WebserverSettings* site2 = reader.getAllServers()[1].get();
    check("multi: block 2 listen port 5000", site2->listen[0].port == 5000);
    check("multi: block 2 root /var/www/site2", site2->root == "/var/www/site2");
    check("multi: block 2 index index.php", site2->index == "index.php");
    check("multi: block 2 server_name api.example.com", hasName(*site2, "api.example.com"));
    check("multi: block 2 missing_content_type DEFAULT",
        site2->missing_content_type_policy == MissingContentTypePolicy::DEFAULT);
    check("multi: block 2 missing_content_type default type",
        site2->missing_content_type_default == "application/json");

    check("multi: block 2 location /upload exists",
        site2->locations.find("/upload") != site2->locations.end());
    const auto& upload = site2->locations.at("/upload");
    check("multi: location /upload upload_dir",
        upload.upload_dir == "/tmp/uploads");
    check("multi: location /upload missing_content_type DEFAULT",
        upload.missing_content_type_policy == MissingContentTypePolicy::DEFAULT);
    check("multi: location /upload default type octet-stream",
        upload.missing_content_type_default == "application/octet-stream");
    check("multi: block 2 location / root",
        site2->locations.at("/").root == "/var/www/site2");

    // getSettings() resolves every server_name to its owning block
    check("multi: getSettings example.com -> block 1",
        reader.getSettings("example.com") == site1);
    check("multi: getSettings api.example.com -> block 2",
        reader.getSettings("api.example.com") == site2);
}

// --------------- same port vhosts config ---------------

static void test_same_port_vhosts() {
    ConfigReader reader("tests/sample_cfg/same_port_vhosts.conf");
    check("same_port_vhosts: 2 server blocks", reader.getAllServers().size() == 2u);

    const WebserverSettings* s1 = reader.getAllServers()[0].get();
    check("same_port_vhosts: s1 listen 8081", s1->listen[0].port == 8081);
    check("same_port_vhosts: s1 default_server", s1->listen[0].is_default == true);
    check("same_port_vhosts: s1 server_name example.com", hasName(*s1, "example.com"));
    check("same_port_vhosts: s1 root www/site1", s1->root == "www/site1");

    const WebserverSettings* s2 = reader.getAllServers()[1].get();
    check("same_port_vhosts: s2 listen 8081", s2->listen[0].port == 8081);
    check("same_port_vhosts: s2 not default_server", s2->listen[0].is_default == false);
    check("same_port_vhosts: s2 server_name api.example.com", hasName(*s2, "api.example.com"));
    check("same_port_vhosts: s2 root www/site2", s2->root == "www/site2");

    check("same_port_vhosts: getSettings example.com -> s1", reader.getSettings("example.com") == s1);
    check("same_port_vhosts: getSettings api.example.com -> s2", reader.getSettings("api.example.com") == s2);
}

// --------------- valid full features config ---------------

static void test_valid_full_features() {
    ConfigReader reader("tests/sample_cfg/valid_full_features.conf");
    check("valid_full_features: 1 server block", reader.getAllServers().size() == 1u);

    const WebserverSettings* ws = reader.getAllServers()[0].get();
    check("valid_full_features: 2 listen directives", ws->listen.size() == 2u);
    check("valid_full_features: listen 0 addr", ws->listen[0].address == "127.0.0.1");
    check("valid_full_features: listen 0 port", ws->listen[0].port == 8080);
    check("valid_full_features: listen 0 is default", ws->listen[0].is_default == true);
    check("valid_full_features: listen 1 port", ws->listen[1].port == 8443);
    check("valid_full_features: listen 1 not default", ws->listen[1].is_default == false);

    check("valid_full_features: server_names count", ws->server_name.size() == 2u);
    check("valid_full_features: server_name full.example.com", hasName(*ws, "full.example.com"));
    check("valid_full_features: server_name www.full.example.com", hasName(*ws, "www.full.example.com"));

    check("valid_full_features: root /var/www/full", ws->root == "/var/www/full");
    check("valid_full_features: index index.html", ws->index == "index.html");
    check("valid_full_features: dirindex on", ws->dirindex == true);
    check("valid_full_features: max_header_size 4096", ws->max_header_size == 4096u);
    check("valid_full_features: max_body_size 2000000", ws->max_body_size == 2000000u);
    check("valid_full_features: error_page 404", ws->error_page.at(404) == "/errors/404.html");
    check("valid_full_features: mct DEFAULT", ws->missing_content_type_policy == MissingContentTypePolicy::DEFAULT);
    check("valid_full_features: mct default application/octet-stream", ws->missing_content_type_default == "application/octet-stream");

    check("valid_full_features: 3 locations", ws->locations.size() == 3u);
    check("valid_full_features: location / methods", ws->locations.at("/").methods.size() == 2u);
    check("valid_full_features: location / cgi_extension", ws->locations.at("/cgi-bin").cgi_extension == ".py");
    check("valid_full_features: location /cgi-bin interpreter", ws->locations.at("/cgi-bin").cgi_ext_interpreter.at(".py") == "/usr/bin/python3");
    check("valid_full_features: location /cgi-bin upload_dir", ws->locations.at("/cgi-bin").upload_dir == "/tmp/uploads");
    check("valid_full_features: location /old redirect", ws->locations.at("/old").redirect == "/new");
}

// --------------- valid outside and unknown config ---------------

static void test_valid_outside_and_unknown() {
    ConfigReader reader("tests/sample_cfg/valid_outside_and_unknown.conf");
    check("valid_outside_and_unknown: 1 server block", reader.getAllServers().size() == 1u);

    const WebserverSettings* ws = reader.getAllServers()[0].get();
    check("valid_outside_and_unknown: listen port 8092", ws->listen[0].port == 8092);
    check("valid_outside_and_unknown: server_name outside.example.com", hasName(*ws, "outside.example.com"));
    check("valid_outside_and_unknown: root /var/www/outside", ws->root == "/var/www/outside");
}

// --------------- valid quoted hash config ---------------

static void test_valid_quoted_hash() {
    ConfigReader reader("tests/sample_cfg/valid_quoted_hash.conf");
    check("valid_quoted_hash: 1 server block", reader.getAllServers().size() == 1u);

    const WebserverSettings* ws = reader.getAllServers()[0].get();
    check("valid_quoted_hash: listen port 8091", ws->listen[0].port == 8091);
    check("valid_quoted_hash: index with hash in quotes", ws->index == "\"index#special.html\"");
}

// --------------- valid split brace config ---------------

static void test_valid_split_brace() {
    ConfigReader reader("tests/sample_cfg/valid_split_brace.conf");
    check("valid_split_brace: 1 server block", reader.getAllServers().size() == 1u);

    const WebserverSettings* ws = reader.getAllServers()[0].get();
    check("valid_split_brace: listen port 8090", ws->listen[0].port == 8090);
    check("valid_split_brace: server_name split.example.com", hasName(*ws, "split.example.com"));
    check("valid_split_brace: root /var/www/split", ws->root == "/var/www/split");
}

// --------------- edge single line location config ---------------

static void test_edge_single_line_location() {
    ConfigReader reader("tests/sample_cfg/edge_single_line_location.conf");
    check("edge_single_line_location: 1 server block", reader.getAllServers().size() == 1u);

    const WebserverSettings* ws = reader.getAllServers()[0].get();
    check("edge_single_line_location: listen port 8106", ws->listen[0].port == 8106);
    check("edge_single_line_location: server_name singleline.example.com", hasName(*ws, "singleline.example.com"));
    check("edge_single_line_location: location / exists", ws->locations.find("/") != ws->locations.end());
}

// --------------- edge stray top level brace config ---------------

static void test_edge_stray_top_level_brace() {
    ConfigReader reader("tests/sample_cfg/edge_stray_top_level_brace.conf");
    check("edge_stray_top_level_brace: 1 server block", reader.getAllServers().size() == 1u);

    const WebserverSettings* ws = reader.getAllServers()[0].get();
    check("edge_stray_top_level_brace: listen port 8107", ws->listen[0].port == 8107);
    check("edge_stray_top_level_brace: server_name stray.example.com", hasName(*ws, "stray.example.com"));
}

// --------------- invalid sample configs ---------------

static void test_invalid_configs() {
    check_throws("duplicate_default.conf rejected", "tests/sample_cfg/duplicate_default.conf");
    check_throws("invalid_cgi_missing_interpreter.conf rejected", "tests/sample_cfg/invalid_cgi_missing_interpreter.conf");
    check_throws("invalid_duplicate_default_server.conf rejected", "tests/sample_cfg/invalid_duplicate_default_server.conf");
    check_throws("invalid_error_page_missing_path.conf rejected", "tests/sample_cfg/invalid_error_page_missing_path.conf");
    check_throws("invalid_header_size_out_of_range.conf rejected", "tests/sample_cfg/invalid_header_size_out_of_range.conf");
    check_throws("invalid_listen_in_location.conf rejected", "tests/sample_cfg/invalid_listen_in_location.conf");
    check_throws("invalid_location_missing_brace.conf rejected", "tests/sample_cfg/invalid_location_missing_brace.conf");
    check_throws("invalid_missing_content_type.conf rejected", "tests/sample_cfg/invalid_missing_content_type.conf");
    check_throws("invalid_missing_listen.conf rejected", "tests/sample_cfg/invalid_missing_listen.conf");
    check_throws("invalid_missing_open_after_server.conf rejected", "tests/sample_cfg/invalid_missing_open_after_server.conf");
    check_throws("invalid_no_server_block.conf rejected", "tests/sample_cfg/invalid_no_server_block.conf");
    check_throws("invalid_partial_failure.conf rejected", "tests/sample_cfg/invalid_partial_failure.conf");
    check_throws("invalid_server_name_in_location.conf rejected", "tests/sample_cfg/invalid_server_name_in_location.conf");
    check_throws("invalid_unclosed_server.conf rejected", "tests/sample_cfg/invalid_unclosed_server.conf");
    check_throws("invalid_unknown_method.conf rejected", "tests/sample_cfg/invalid_unknown_method.conf");
    check_throws("invalid_unmatched_close.conf rejected", "tests/sample_cfg/invalid_unmatched_close.conf");
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

    std::cout << "Same Port Vhosts:" << std::endl;
    test_same_port_vhosts();
    std::cout << std::endl;

    std::cout << "Valid Full Features:" << std::endl;
    test_valid_full_features();
    std::cout << std::endl;

    std::cout << "Valid Outside and Unknown:" << std::endl;
    test_valid_outside_and_unknown();
    std::cout << std::endl;

    std::cout << "Valid Quoted Hash:" << std::endl;
    test_valid_quoted_hash();
    std::cout << std::endl;

    std::cout << "Valid Split Brace:" << std::endl;
    test_valid_split_brace();
    std::cout << std::endl;

    std::cout << "Edge Single Line Location:" << std::endl;
    test_edge_single_line_location();
    std::cout << std::endl;

    std::cout << "Edge Stray Top Level Brace:" << std::endl;
    test_edge_stray_top_level_brace();
    std::cout << std::endl;

    std::cout << "Invalid Configs Validation:" << std::endl;
    test_invalid_configs();

    std::cout << "\n----------------------------------------" << std::endl;
    std::cout << "Results: " << g_passed << " passed, " << g_failed << " failed, "
              << (g_passed + g_failed) << " total" << std::endl;
    return g_failed > 0 ? 1 : 0;
}

