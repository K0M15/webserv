#pragma once

#include <string>
#include <vector>
#include <functional>
#include <sys/types.h>
#include "Request.hpp"
#include "Connection.hpp"

// https://www.php.net/security.cgi-bin

class CGIHandler{
private:
    CGIHandler();
    void setEnv();
    void spawnCGI();
    void finish();
    const std::string m_filePath;
    const std::string m_iPath;
    const Request m_req;
    const Connection& m_conn;
    std::vector<std::string> m_env_strings;   // owns the "NAME=value" memory
    std::vector<char*> m_env;                 // points into m_env_strings
    std::string m_output;
    int m_exitStatus;
    pid_t m_pid;
    bool m_done;
    bool m_output_drained;
    bool m_status_collected;
    std::function<void()> m_onComplete;
public:
    CGIHandler(
        const std::string filePath,
        const std::string iPath,
        const Request req,
        const Connection& conn,
        std::function<void()> onComplete
    ) : m_filePath(filePath), m_iPath(iPath), m_req(req), m_conn(conn),
        m_env_strings(), m_env(), m_output(), m_exitStatus(0), m_pid(-1),
        m_done(false), m_output_drained(false), m_status_collected(false),
        m_onComplete(onComplete)
    {
        setEnv();
        spawnCGI();
    };
    ~CGIHandler();

    const std::string& getOutput() const { return m_output; }
    int getExitStatus() const { return m_exitStatus; }
    bool isDone(){ return m_done; }
};
