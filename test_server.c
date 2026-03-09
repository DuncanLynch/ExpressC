#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ExpressC.h"

static void set_text_plain(http_response* res) {
    (void)set_response_header(res, "Content-Type", "text/plain; charset=utf-8");
}

typedef struct serverctx {
    char lastmessage[256];
    size_t message_len;
    int numbers[5];
} serverctx;


static void index_handler(void* ctx, http_request* req, http_response* res) {
    (void)ctx;
    (void)req;

    static const char body[] =
        "ExpressC test server\n"
        "GET /ping\n"
        "POST /echo\n";

    set_text_plain(res);
    (void)set_response_body(res, (const byte*)body, sizeof(body) - 1);
}

static void ping_handler(void* ctx, http_request* req, http_response* res) {
    (void)ctx;
    (void)req;

    static const char body[] = "pong\n";
    set_text_plain(res);
    (void)set_response_body(res, (const byte*)body, sizeof(body) - 1);
}

static void echo_handler(void* ctx, http_request* req, http_response* res) {
    (void)ctx;

    const byte* body = get_request_body(req);
    size_t body_len = get_request_body_len(req);
    char* content_type = get_request_content_type(req);

    if (content_type != NULL) {
        (void)set_response_header(res, "Content-Type", content_type);
    } else {
        (void)set_response_header(res, "Content-Type", "application/octet-stream");
    }

    (void)set_response_body(res, body, body_len);
}

static void post_message_handler(void* ctx, http_request* req, http_response* res) {
    serverctx* context = (serverctx*) ctx;
    const byte* body = get_request_body(req);
    size_t body_len = get_request_body_len(req);
    char* content_type = get_request_content_type(req);

    if (strcmp(content_type, "text/plain") != 0 || body_len > 255) {
        (void)set_response_status(res, "400");
        return;
    }

    memcpy(context->lastmessage, body, body_len);
    context->lastmessage[body_len] = '\0';
    context->message_len = body_len;
}

static void get_message_handler(void* ctx, http_request* req, http_response* res) {
    serverctx* context = (serverctx*) ctx;
    (void)req;

    set_response_body(res, (byte*)context->lastmessage,  context->message_len);
}

int main(int argc, char** argv) {
    uint16_t port = 8080;
    if (argc >= 2) {
        long parsed = strtol(argv[1], NULL, 10);
        if (parsed > 0 && parsed <= 65535) port = (uint16_t)parsed;
    }

    signal(SIGINT, SIG_DFL);

    ExpressRouter* router = router_new();
    if (router == NULL) {
        fprintf(stderr, "router_new failed\n");
        return 1;
    }

    if (router_add(router, (char*)"/", GET, index_handler) != 0 ||
        router_add(router, (char*)"/ping", GET, ping_handler) != 0 ||
        router_add(router, (char*)"/echo", POST, echo_handler) != 0 ||
        router_add(router, (char*)"/message", GET, get_message_handler) != 0 ||
        router_add(router, (char*)"/message", POST, post_message_handler)
    ) {
        fprintf(stderr, "router_add failed\n");
        router_destroy(router);
        return 1;
    }

    ExpressConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.port = port;

    ExpressServer* server = server_new(&cfg, router);
    if (server == NULL) {
        fprintf(stderr, "server_new failed\n");
        router_destroy(router);
        return 1;
    }

    fprintf(stderr, "ExpressC test server listening on 0.0.0.0:%u\n", port);
    server_run(server);

    server_destroy(server);
    router_destroy(router);
    return 0;
}
