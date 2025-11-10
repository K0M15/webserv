#pragma once

#include "BaseRequestHandler.h"
#include <iostream>
#include <filesystem>

class FTRequestHandler : BaseRequestHandler{
public:
    FTRequestHandler(Request request, std::filesystem::path cgi_dir, std::filesystem::path static_dir);
    virtual int do_GET();
    virtual int do_HEAD();
    virtual int do_POST();
    virtual int do_PUT();
    virtual int do_DELETE();
    // virtual int do_CONNECT();
    virtual int do_OPTIONS();
    virtual int do_TRACE();
    // virtual int do_PATCH();
private:
    std::filesystem::path   cgi_directory;
    std::filesystem::path   static_directory;
};
