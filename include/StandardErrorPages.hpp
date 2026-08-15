#pragma once

#include <string>
#include <unordered_map>
#include "HttpStatusReason.hpp"

namespace HtmlPages{

constexpr std::string_view raw_template = R"(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>{STATUS_CODE} - {REASON}</title>
</head>
<body style="
    text-align: center;
    margin-top: 30%;
    height: 100%;"
    >
    <h1>{STATUS_CODE} - {REASON}</h1>
    <div id="sketchy_explain">Loading excuse...</div>

    <script>
    const data = [
        "A stray cosmic ray flipped a bit in our server's RAM. We are currently trying to flip it back.",
        "The server rack caught fire, but on the bright side, the lead dev made s'mores.",
        "A intern tripped over the power cord while trying to fetch coffee.",
        "Our backend developer swears this worked on their local machine.",
        "The server is currently undergoing a mid-life crisis. Please give it a moment.",
        "We tried fixing the bug, but three more appeared in its place.",
        "A hamster powering our main database server took an unscheduled nap.",
        "We deleted a load-bearing comment in the source code by accident.",
        "The server received your request and decided it simply didn't feel like answering."
    ];

    const randomIndex = Math.floor(Math.random() * data.length);
    document.getElementById("sketchy_explain").innerHTML = data[randomIndex];
    </script>
</body>
</html>
)";

inline std::string construct_errorpage(const unsigned int status_code, const std::string& reason) {
    std::string result(raw_template);
    std::string key = "{STATUS_CODE}";
    std::string value = std::to_string(status_code);
    size_t pos = 0;
    while ((pos = result.find(key, pos)) != std::string::npos) {
        result.replace(pos, key.length(), value);
        pos += value.length();
    }
    key = "{REASON}";
    pos = 0;
    while ((pos = result.find(key, pos)) != std::string::npos) {
        result.replace(pos, key.length(), reason);
        pos += reason.length();
    }
    return result;
}

};