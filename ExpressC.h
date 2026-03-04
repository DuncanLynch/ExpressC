#pragma once
#include <stdint.h>

typedef struct ExpressServer ExpressServer;

typedef struct ExpressRouter ExpressRouter;

typedef struct ExpressConfig {
    void* ctx;
    uint16_t port;
} ExpressConfig;

typedef struct http_request http_request;

typedef struct http_response http_response;

typedef void (*route_handler)(void* ctx, http_request *req, http_response *res); 

