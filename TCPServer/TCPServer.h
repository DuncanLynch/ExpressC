#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// macros
#define LISTEN_BACKLOG 512
#define MAX_EVENTS 1024
#define BUF_CAP 65536

typedef unsigned char byte;

typedef struct TCPServer TCPServer;

typedef struct TCPConn TCPConn;

typedef void (*tcp_on_bytes_fn)(void* ctx, TCPConn* c, const byte* data,
                                size_t len);

typedef void (*tcp_on_accept_fn)(void* ctx, TCPConn* conn);
typedef void (*tcp_on_close_fn)(void* ctx, TCPConn* c);

typedef struct TCPServerConfig {
    tcp_on_accept_fn on_accept;
    tcp_on_bytes_fn on_bytes;
    tcp_on_close_fn on_close;
    uint16_t port;
    void* ctx;
} TCPServerConfig;

TCPServer* tcp_server_create(const TCPServerConfig* cfg);
int tcp_server_run(TCPServer* s);
void tcp_server_destroy(TCPServer* s);

bool tcp_conn_write(TCPConn* c, const void* data, size_t len);
bool tcp_conn_write_str(TCPConn* c, const char* s);

void tcp_conn_close_after_write(TCPConn* c);
void tcp_conn_close_now(TCPConn* c);

void tcp_conn_set_user(TCPConn* c, void* user);
void* tcp_conn_get_user(TCPConn* c);

const char* tcp_conn_ip(const TCPConn* c);
uint16_t tcp_conn_port(const TCPConn* c);
