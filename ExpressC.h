#pragma once
#include <stdint.h>
#include <types.h>
#include <stdbool.h>

enum Method {
    GET = 0,
    POST = 1,
    DELETE = 2,
    PUT = 3,
    PATCH = 4,
    HEAD = 5,
};

typedef struct ExpressServer ExpressServer;

typedef struct ExpressRouter ExpressRouter;

typedef struct ExpressConfig {
    void* ctx;
    uint16_t port;
} ExpressConfig;

typedef struct http_request http_request;

typedef struct http_response http_response;

typedef void (*route_handler)(void* ctx, http_request* req, http_response* res);

typedef void (*middleware_handler)(void* ctx, http_request* req,
                                   http_response* res);

ExpressRouter* router_new();
int32_t router_add(ExpressRouter* r, char* route, enum Method method,
                   route_handler routing_func);
int32_t router_add_middleware(ExpressRouter* r, middleware_handler mware_func);
void router_destroy(ExpressRouter* r);

ExpressServer* server_new(ExpressConfig* cnfg, ExpressRouter* router);
void server_run(ExpressServer* server);
void server_destroy(ExpressServer* server);

param* get_request_param(http_request* req, const char* key);
param* get_request_route_param(http_request* req, const char* key);
header* get_request_header(http_request* req, const char* key);
byte* get_request_body(http_request* req);
size_t get_request_body_len(http_request* req);
char* get_request_content_type(http_request* req);

header* get_response_header(http_response* res, const char* key);
bool set_response_header(http_response* res, const char* key, const char* value);
bool set_response_body(http_response* res, const byte* body, const size_t body_len);
bool set_response_status(http_response* res, const char* status);
