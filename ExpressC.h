#pragma once
#include <stdint.h>

enum Method {
    GET = 0,
    POST = 1,
    DELETE = 2,
    PUT = 3,
    PATCH = 4,
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
