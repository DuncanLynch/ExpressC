#include "ExpressC.h"

#include <stdlib.h>
#include <string.h>

#include "TCPServer/TCPServer.h"
#include "types.h"

#define MAX_METHODS 5
#define MAX_ROUTES 512

struct HTTPConn {
    byte* bytes;
    size_t bytes_len;
    size_t bytes_off;
    size_t bytes_cap;

    http_request* req;
};

struct MethodRoute {
    route_handler handler;
};

struct Route {
    struct MethodRoute handlers[MAX_METHODS];
    char* route;
};

typedef struct ExpressRouter {
    struct Route routes[MAX_ROUTES];
    size_t routes_off;
    middleware_handler mware_func;
} ExpressRouter;

typedef struct ExpressServer {
    void* user_ctx;
    ExpressRouter* router;

    size_t total_requests;

} ExpressServer;

// TODO: Finish this
enum Method get_method_from_str(char* method) {
    if (strcmp(method, "GET")) {
        return GET;
    } else if (strcmp(method, "POST")) {
        return POST;
    } else {
        return GET;
    }
}

struct Route* find_route(ExpressRouter* r, char* route) {
    for (int i = 0; i < MAX_ROUTES; i++) {
        if (r->routes[i].route != NULL &&
            strcmp(route, r->routes[i].route) == 0) {
            return &r->routes[i];
        }
    }
    return NULL;
}

ExpressRouter* router_new() {
    ExpressRouter* r = (ExpressRouter*)calloc(1, sizeof(ExpressRouter));
    r->mware_func = NULL;
    return r;
}

int32_t router_add_middleware(ExpressRouter* r, middleware_handler mware_func) {
    if (r->mware_func != NULL) return -1;
    r->mware_func = mware_func;
    return 0;
}

// TODO: Refactor routes to be heap allocated. (arenas?)
int32_t router_add(ExpressRouter* r, char* route, enum Method method,
                   route_handler routing_func) {
    if (r->routes_off >= MAX_ROUTES || method >= MAX_METHODS) return -1;
    struct Route* t = find_route(r, route);
    if (t == NULL) {
        r->routes[r->routes_off].route = route;
        r->routes[r->routes_off].handlers[method].handler = routing_func;
        r->routes_off++;
    } else {
        if (t->handlers[method].handler != NULL) return -1;
        t->handlers[method].handler = routing_func;
    }
    return 0;
}

void router_destroy(ExpressRouter* r) { free(r); }

void on_accept(void* ctx, TCPConn* c) {}

void on_bytes(void* ctx, TCPConn* c, const byte* bytes, size_t len) {
    struct HTTPConn* conn = (struct HTTPConn*)tcp_conn_get_user(c);
    ExpressServer* s = (ExpressServer*)ctx;
    // TODO: Parse HTTP message
    http_request* req = conn->req;
    http_response* res = NULL;  // TODO: decide how to initialize this
    struct Route* r = find_route(s->router, req->route);
    r->handlers[get_method_from_str(req->method)].handler(s->user_ctx, req,
                                                          res);
}
