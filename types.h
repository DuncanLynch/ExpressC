#pragma once
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

typedef u_int16_t byte;

typedef struct params {
    char* key;
    char* value;
} params;

typedef struct header {
    char* key;
    char* value;
} header;

typedef struct http_request {
    char* route;
    char* method;
    params* request_params;
    size_t request_params_len;
    params* route_params;
    size_t route_params_len;
    header* headers;
    size_t headers_len;
    char* host;
    size_t content_length;
    char* content_type;
    byte* body;
} http_request;

typedef struct http_response {
    header* headers;
    size_t headers_len;
    size_t content_length;
    byte* body;
    char* status_code;
} http_response;
