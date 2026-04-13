#pragma once
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#define MAX_HEADERS 128
#define MAX_PARAMS 128

typedef uint8_t byte;

typedef struct param {
    char* key;
    char* value;
} param;

typedef struct header {
    char* key;
    char* value;
} header;

typedef struct http_request {
    char route[1024];
    char method[16];
    char version[16];
    param request_params[MAX_PARAMS];
    size_t request_params_len;
    param route_params[MAX_PARAMS];
    size_t route_params_len;
    header headers[MAX_HEADERS];
    size_t headers_len;
    char* host;
    size_t content_length;
    char* content_type;
    byte* body;
    bool chunked;
} http_request;

typedef struct http_response {
    header* headers;
    size_t headers_len;
    size_t content_length;
    byte* body;
    char* status_code;
} http_response;
