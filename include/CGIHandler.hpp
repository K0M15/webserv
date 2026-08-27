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
    std::string m_scriptName;
    std::string m_pathInfo;
    std::string m_pathTranslated;
    std::vector<std::string> m_env_strings;   // owns the "NAME=value" memory
    std::vector<char*> m_env;                 // points into m_env_strings
    std::string m_output;
    size_t m_maxOutputSize;
    time_t m_startTime;
    int m_exitStatus;
    pid_t m_pid;
    int m_stdin_fd;
    int m_stdout_fd;
    bool m_done;
    bool m_output_drained;
    bool m_status_collected;
    bool m_output_exceeded;
    bool m_timed_out;
    std::function<void()> m_onComplete;
    size_t m_input_write_offset;
public:
    CGIHandler(
        const std::string filePath,
        const std::string iPath,
        const Request req,
        const Connection& conn,
        std::function<void()> onComplete,
        const std::string scriptName = "",
        const std::string pathInfo = "",
        const std::string pathTranslated = ""
    );
    ~CGIHandler();

    const std::string& getOutput() const { return m_output; }
    int getExitStatus() const { return m_exitStatus; }
    const std::string& getScriptName() const { return m_scriptName; }
    const std::string& getPathInfo() const { return m_pathInfo; }
    const std::string& getPathTranslated() const { return m_pathTranslated; }
    bool isDone() const { return m_done; }
    bool isOutputExceeded() const { return m_output_exceeded; }
    bool isTimedOut() const { return m_timed_out; }
    bool checkTimeout(int timeout_seconds = 10);
    void killProcess();
};
