#pragma once

#include <string>
#include <vector>
#include "Request.hpp"
#include "Connection.hpp"

// https://www.php.net/security.cgi-bin

class CGIHandler{
private:
    CGIHandler();
    void setEnv();
    void spawnCGI();
    const std::string m_filePath;
    const std::string m_iPath;
    const Request m_req;
    const Connection m_conn;
    std::vector<std::string> m_env_strings;   // owns the "NAME=value" memory
    std::vector<char*> m_env;                 // points into m_env_strings
    std::string m_output;                     // CGI stdout captured by spawnCGI
    int m_exitStatus;
public:
    CGIHandler(
        const std::string filePath,
        const std::string iPath,
        const Request req,
        const Connection conn
    ) : m_filePath(filePath), m_iPath(iPath), m_req(req), m_conn(conn),
        m_env_strings(), m_env(), m_output(), m_exitStatus(0)
    {
        setEnv();
        spawnCGI();
    };
    const std::string& getOutput() const { return m_output; }
    int getExitStatus() const { return m_exitStatus; }
    ~CGIHandler();
};
