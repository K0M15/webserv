
// https://web.archive.org/web/20100123121442/http://hoohoo.ncsa.illinois.edu/cgi/env.html

struct ICGIEnv
{
    char*   AUTH_TYPE;
    char*   CONTENT_LENGTH;
    char*   CONTENT_TYPE;
    char*   GATEWAY_INTERFACE;
    char*   HTTP_ACCEPT;
    char*   HTTP_USER_AGENT;
    char*   PATH_INFO;
    char*   PATH_TRANSLATED;
    char*   QUERY_STRING;
    char*   REMOTE_ADDR;
    char*   REMOTE_HOST;
    char*   REMOTE_IDENT;
    char*   REMOTE_USER;
    char*   REQUEST_METHOD;
    char*   SCRIPT_NAME;
    char*   SERVER_NAME;
    char*   SERVER_PORT;
    char*   SERVER_PROTOCOL;
    char*   SERVER_SOFTWARE;
}

/*
    - create env var
    - find script file and look out for shebang or open based on filetype. check permissions?
    std::filesystem::status("filename").permissions()
    - call php or other cgi with execve
    /usr/bin/env php-CGI or /usr/bin/env perl or /usr/bin/env python3
    - probably wont work? find the executable in path   
*/

