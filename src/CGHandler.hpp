#pragma once

#include <string>
#include "Request.hpp"
#include "Connection.hpp"

class CGIHandler{
private:
    CGIHandler();
    void setEnv();
    void spawnRemote();
    const std::string m_filePath;
    const std::string m_iPath;
    const Request m_req;
    const Connection m_conn;
    char *env;
public:
    CGIHandler(
        const std::string filePath,
        const std::string iPath,
        const Request req,
        const Connection conn
    ) : m_filePath(filePath), m_iPath(iPath), m_req(req), m_conn(conn){
        setEnv();
        // prepare fds? closexec?
        spawnRemote(); //setup pipes
    };
    ~CGIHandler();
}