#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "TCPServer.h"

typedef struct {
    size_t total_bytes;
} ConnState;

static void on_accept(void* ctx, TCPConn* c) {
    (void)ctx;

    fprintf(stderr, "[accept] %s:%u\n", tcp_conn_ip(c), tcp_conn_port(c));

    ConnState* st = (ConnState*)calloc(1, sizeof(*st));
    tcp_conn_set_user(c, st);

    tcp_conn_write_str(c, "echo server ready\r\n");
}

static int line_is(const byte* data, size_t len, const char* s) {
    size_t sl = strlen(s);

    while (len > 0 && (data[len - 1] == '\n' || data[len - 1] == '\r')) {
        len--;
    }
    if (len != sl) return 0;
    return memcmp(data, s, sl) == 0;
}

static void on_bytes(void* ctx, TCPConn* c, const byte* data, size_t len) {
    (void)ctx;
    fprintf(stderr, "[bytes] %s:%u\n", tcp_conn_ip(c), tcp_conn_port(c));
    ConnState* st = (ConnState*)tcp_conn_get_user(c);
    if (st) st->total_bytes += len;

    if (line_is(data, len, "quit") || line_is(data, len, "exit")) {
        tcp_conn_write_str(c, "bye\r\n");
        tcp_conn_close_after_write(c);
        return;
    }
    if (line_is(data, len, "close")) {
        tcp_conn_close_now(c);
        return;
    }

    if (!tcp_conn_write(c, data, len)) {
        tcp_conn_close_now(c);
        return;
    }
}

static void on_close(void* ctx, TCPConn* c) {
    (void)ctx;

    ConnState* st = (ConnState*)tcp_conn_get_user(c);
    size_t total = st ? st->total_bytes : 0;

    fprintf(stderr, "[close ] %s:%u total_bytes=%zu\n", tcp_conn_ip(c),
            tcp_conn_port(c), total);

    free(st);
}

int main(int argc, char** argv) {
    uint16_t port = 12345;
    if (argc >= 2) {
        long p = strtol(argv[1], NULL, 10);
        if (p > 0 && p <= 65535) port = (uint16_t)p;
    }

    signal(SIGINT, SIG_DFL);

    TCPServerConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.port = port;
    cfg.on_accept = on_accept;
    cfg.on_bytes = on_bytes;
    cfg.on_close = on_close;
    cfg.ctx = NULL;

    TCPServer* s = tcp_server_create(&cfg);
    if (!s) {
        perror("tcp_server_create");
        return 1;
    }

    fprintf(stderr, "Echo server listening on 0.0.0.0:%u\n", port);
    int rc = tcp_server_run(s);

    tcp_server_destroy(s);
    return rc;
}
