#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../ExpressC.h"

typedef struct {
    size_t requests;
    ExpressServer* server;
    const char* public_path;
} ServerCtx;

static void count_requests(void* ctx, http_request* req, http_response* res) {
    (void)req; (void)res;
    ((ServerCtx*)ctx)->requests++;
}

static void api_ping(void* ctx, http_request* req, http_response* res) {
    (void)ctx; (void)req;
    static const char body[] = "pong\n";
    set_response_header(res, "Content-Type", "text/plain; charset=utf-8");
    set_response_body(res, (const byte*)body, sizeof(body) - 1);
}

static void api_echo(void* ctx, http_request* req, http_response* res) {
    (void)ctx;
    const byte* body = get_request_body(req);
    size_t len = get_request_body_len(req);
    char* ct = get_request_content_type(req);
    set_response_header(res, "Content-Type", ct ? ct : "application/octet-stream");
    set_response_body(res, body, len);
}

static void api_stats(void* ctx, http_request* req, http_response* res) {
    (void)req;
    ServerCtx* s = ctx;
    static char body[128];
    int n = snprintf(body, sizeof(body),
        "{\"requests\":%zu,\"files\":%zu}",
        s->requests, server_static_file_count(s->server));
    set_response_header(res, "Content-Type", "application/json");
    set_response_body(res, (byte*)body, (size_t)n);
}

static void spa_fallback(void* ctx, http_request* req, http_response* res) {
    (void)req;
    ServerCtx* s = ctx;
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/index.html", s->public_path);
    FILE* f = fopen(path, "rb");
    if (!f) { set_response_status(res, "404"); return; }
    static byte buf[524288];
    size_t n = fread(buf, 1, sizeof(buf), f);
    fclose(f);
    set_response_header(res, "Content-Type", "text/html; charset=utf-8");
    set_response_body(res, buf, n);
}

int main(int argc, char** argv) {
    uint16_t port = 8080;
    const char* public_path = "./test/public";

    if (argc >= 2) {
        long p = strtol(argv[1], NULL, 10);
        if (p > 0 && p <= 65535) port = (uint16_t)p;
    }
    if (argc >= 3) public_path = argv[2];

    signal(SIGINT, SIG_DFL);

    ExpressRouter* router = router_new();
    if (!router) { fprintf(stderr, "router_new failed\n"); return 1; }

    ServerCtx ctx = { .requests = 0, .server = NULL, .public_path = public_path };

    if (router_add_middleware(router, count_requests) != 0 ||
        router_add(router, (char*)"/api/ping",  GET,  api_ping)    != 0 ||
        router_add(router, (char*)"/api/echo",  POST, api_echo)    != 0 ||
        router_add(router, (char*)"/api/stats", GET,  api_stats)   != 0 ||
        router_set_fallback(router, spa_fallback) != 0) {
        fprintf(stderr, "router setup failed\n");
        router_destroy(router);
        return 1;
    }

    ExpressConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.port        = port;
    cfg.ctx         = &ctx;
    cfg.public_path = public_path;

    ExpressServer* server = server_new(&cfg, router);
    if (!server) {
        fprintf(stderr, "server_new failed\n");
        router_destroy(router);
        return 1;
    }
    ctx.server = server;

    fprintf(stderr, "listening on http://0.0.0.0:%u\n", port);
    fprintf(stderr, "serving %s (%zu files indexed)\n",
            public_path, server_static_file_count(server));
    server_run(server);

    server_destroy(server);
    router_destroy(router);
    return 0;
}
