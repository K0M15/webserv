#pragma once

// Application Metadata
#ifndef APPLICATION_NAME
# define APPLICATION_NAME "webserv"
#endif

#ifndef APPLICATION_VERSION
# define APPLICATION_VERSION "0.1"
#endif

#ifndef APPLICATION_ID
# define APPLICATION_ID APPLICATION_NAME "/" APPLICATION_VERSION
#endif

// HTTP Protocol Constants
#ifndef HTTP_VERSION
# define HTTP_VERSION "HTTP/1.1"
#endif

#ifndef CGI_GATEWAY_INTERFACE
# define CGI_GATEWAY_INTERFACE "CGI/1.1"
#endif

// Default Server Settings and Addresses
#ifndef DEFAULT_LISTEN_ADDRESS
# define DEFAULT_LISTEN_ADDRESS "0.0.0.0"
#endif

#ifndef DEFAULT_INDEX_FILE
# define DEFAULT_INDEX_FILE "index.html"
#endif

// Default Limits
#ifndef DEFAULT_MAX_CGI_OUTPUT
# define DEFAULT_MAX_CGI_OUTPUT 2e6
#endif

#ifndef DEFAULT_MAX_BODY_SIZE
# define DEFAULT_MAX_BODY_SIZE 1e6
#endif

#ifndef DEFAULT_MAX_HEADER_SIZE
# define DEFAULT_MAX_HEADER_SIZE 16384UL
#endif

// Validation Bounds for Settings
#ifndef MIN_MAX_HEADER_SIZE
# define MIN_MAX_HEADER_SIZE 50UL
#endif

#ifndef MAX_MAX_HEADER_SIZE
# define MAX_MAX_HEADER_SIZE 8192UL
#endif

#ifndef MIN_MAX_BODY_SIZE
# define MIN_MAX_BODY_SIZE 50UL
#endif

#ifndef MAX_MAX_BODY_SIZE
# define MAX_MAX_BODY_SIZE (100UL * 1024UL * 1024UL)
#endif

// Timeout Defaults
#ifndef DEFAULT_KEEP_ALIVE_TIMEOUT
# define DEFAULT_KEEP_ALIVE_TIMEOUT 60
#endif

#ifndef DEFAULT_CGI_TIMEOUT
# define DEFAULT_CGI_TIMEOUT 10
#endif

// Buffer Sizes
#ifndef CGI_BUFFER_SIZE
# define CGI_BUFFER_SIZE 4096
#endif

// FLAGS AUTH
#ifdef DISABLE_AUTH
# define DISABLE_DELETE_AUTH
# define DISABLE_PATCH_AUTH
# define DISABLE_PUT_AUTH
#endif

// #define DISABLE_DELETE_AUTH
// #define DISABLE_PATCH_AUTH
// #define DISABLE_PUT_AUTH
#define REAPPLY_SET_FLAGS        // Rule in subject against F_GETFD, F_GETFL for MACOS
#define SETFD_ALLOWED            // Rule in subject against SETFD (which disables FD_CLOEXEC, which is allowed) for MACOS

// Debug Flag
// # define DEBUG
