#include "TCPServer.h"
//Imports
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <errno.h>
#include <fcntl.h>
#include <signal.h>

struct TCPConn {
    int fd;
    uint8_t out[BUF_CAP];
    size_t out_len;
    size_t out_off;

    char ip[INET_ADDRSTRLEN];
    uint16_t port;

    bool want_write;
    bool close_after_write;
    void* user;
};



static int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) return -1;
    return 0;
}

static void die(const char *msg) {
    perror(msg);
    exit(1);
}

static TCPConn *conn_create(int fd) {
    TCPConn *c = (TCPConn *)calloc(1, sizeof(*c));
    if (!c) return NULL;
    c->fd = fd;
    return c;
}


static void conn_close(int epfd, TCPConn *c) {
    if (!c) return;
    epoll_ctl(epfd, EPOLL_CTL_DEL, c->fd, NULL);
    close(c->fd);
    free(c);
}

static int add_epoll(int epfd, int fd, uint32_t events, void *ptr) {
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = events;
    ev.data.ptr = ptr;
    return epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
}

static int mod_epoll(int epfd, int fd, uint32_t events, void *ptr) {
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = events;
    ev.data.ptr = ptr;
    return epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev);
}

static int create_listen_socket(uint16_t port) {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == -1) return -1;

    int yes = 1;
    (void)setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    #ifdef SO_REUSEPORT
    (void)setsockopt(s, SOL_SOCKET, SO_REUSEPORT, &yes, sizeof(yes));
    #endif

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        close(s);
        return -1;
    }
    if (listen(s, LISTEN_BACKLOG) == -1) {
        close(s);
        return -1;
    }
    return s;
}

static void accept_loop(int epfd, int listen_fd, TCPServerConfig* config) {
    for (;;) {
        struct sockaddr_in in_addr;
        socklen_t in_len = sizeof(in_addr);
        int cfd = accept4(listen_fd, (struct sockaddr *)&in_addr, &in_len, SOCK_NONBLOCK);
        if (cfd == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) break; 
        if (errno == EINTR) continue;
        perror("accept4");
        break;
        }

        TCPConn *c = conn_create(cfd);
        if (!c) {
        close(cfd);
        continue;
        }

        uint32_t ev = EPOLLIN | EPOLLRDHUP | EPOLLHUP | EPOLLERR;
        if (add_epoll(epfd, cfd, ev, c) == -1) {
        perror("epoll_ctl ADD client");
        conn_close(epfd, c);
        continue;
        }

        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &in_addr.sin_addr, ip, sizeof(ip));
        fprintf(stderr, "accepted %s:%u (fd=%d)\n", ip, ntohs(in_addr.sin_port), cfd);
        snprintf(c->ip, sizeof(c->ip), "%s", ip);
        c->port = ntohs(in_addr.sin_port);
        config->on_accept(config->ctx, c);
    }
}

static bool flush_out(int epfd, TCPConn *c) {
    while (c->out_off < c->out_len) {
        ssize_t n = send(c->fd, c->out + c->out_off, c->out_len - c->out_off, 0);
        if (n > 0) {
        c->out_off += (size_t)n;
        continue;
        }
        if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        uint32_t ev = EPOLLIN | EPOLLOUT | EPOLLRDHUP | EPOLLHUP | EPOLLERR;
        (void)mod_epoll(epfd, c->fd, ev, c);
        return true;
        }
        return false;
    }

    c->out_len = c->out_off = 0;
    uint32_t ev = EPOLLIN | EPOLLRDHUP | EPOLLHUP | EPOLLERR;
    (void)mod_epoll(epfd, c->fd, ev, c);
    return true;
}

static bool handle_read(int epfd, TCPConn *c, TCPServerConfig *config) {
    uint8_t buf[4096];

    for (;;) {
        ssize_t n = recv(c->fd, buf, sizeof(buf), 0);

        if (n > 0) {
            if (config->on_bytes) {
                config->on_bytes(config->ctx, c, buf, (size_t)n);
            }
            continue;
        }

        if (n == 0) {
            return false;
        }

        if (errno == EINTR) {
            continue;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            break;
        }

        return false;
    }

    if (c->out_len > c->out_off) {
        if (!flush_out(epfd, c)) {
            return false;
        }
    }

    return true;
}

static bool handle_write(int epfd, struct conn *c) {
    if (c->out_len > c->out_off) {
        return flush_out(epfd, c);
    }
    uint32_t ev = EPOLLIN | EPOLLRDHUP | EPOLLHUP | EPOLLERR;
    (void)mod_epoll(epfd, c->fd, ev, c);
    return true;
}

struct TCPServer {
    int server_fd;
    int epfd;
    tcp_on_accept_fn on_accept;
    tcp_on_bytes_fn on_bytes;
    tcp_on_close_fn on_close;
    uint16_t port;
    void* ctx;
    struct epoll_event events[MAX_EVENTS];
};

TCPServer* tcp_server_create(const TCPServerConfig* cnfg) {
    TCPServer *server = (TCPServer *)calloc(1, sizeof(TCPServer));
    if (!server) return NULL;

    int listen_fd = create_listen_socket(port);
    if (listen_fd == -1) die("create_listen_socket");

    server->server_fd = listen_fd;

    if (set_nonblocking(listen_fd) == -1) die("set_nonblocking(listen_fd)");

    int epfd = epoll_create1(EPOLL_CLOEXEC);
    if (epfd == -1) die("epoll_create1");

    server->epfd = epfd;

    server->port = cnfg->port;
    server->on_accept = cnfg->on_accept;
    server->on_bytes = cnfg->on_bytes;
    server->on_close = cnfg->on_close;

    return server;
}

int tcp_server_run(TCPServer *s) {
    if (!s) return -1;

    signal(SIGPIPE, SIG_IGN);

    for (;;) {
        int n = epoll_wait(s->epfd, s->events, MAX_EVENTS, -1);
        if (n == -1) {
            if (errno == EINTR) continue;
            die("epoll_wait");
        }

        for (int i = 0; i < n; i++) {
            uint32_t e = s->events[i].events;

            if (s->events[i].data.fd == s->server_fd) {
                accept_loop(s);
                continue;
            }

            TCPConn *c = (TCPConn *)s->events[i].data.ptr;
            if (!c) continue;

            if (e & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
                conn_close(c);
                continue;
            }

            if (e & EPOLLIN) {
                if (!handle_read(c)) {
                    conn_close(c);
                    continue;
                }
            }

            if (e & EPOLLOUT) {
                if (!handle_write(c)) {
                    conn_close(c);
                    continue;
                }
            }

            if (c->close_now) {
                conn_close(c);
                continue;
            }
            if (c->close_after_write && c->out_len == c->out_off) {
                conn_close(c);
                continue;
            }
        }
    }

    return 0;
}

void tcp_server_destroy(TCPServer *s) {
    if (!s) return;
    if (s->epfd != -1) close(s->epfd);
    if (s->server_fd != -1) close(s->server_fd);
    free(s);
}

bool tcp_conn_write(TCPConn *c, const void *data, size_t len) {
    if (!c || !data) return false;
    if (len == 0) return true;
    if (c->close_now) return false;

    // If buffer empty, try direct send first (fast path)
    if (c->out_len == c->out_off) {
        c->out_len = 0;
        c->out_off = 0;

        ssize_t n = send(c->fd, data, len, 0);
        if (n == (ssize_t)len) {
            return true;
        }
        if (n < 0) {
            if (!(errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
                return false;
            }
            n = 0;
        }

        // buffer the remainder
        size_t off = (size_t)n;
        size_t rem = len - off;
        if (rem > BUF_CAP) return false;
        memcpy(c->out, (const uint8_t *)data + off, rem);
        c->out_len = rem;
        c->out_off = 0;

        // ensure EPOLLOUT interest
        uint32_t ev = EPOLLIN | EPOLLOUT | EPOLLRDHUP | EPOLLHUP | EPOLLERR;
        (void)mod_epoll(c->server->epfd, c->fd, ev, c);
        return true;
    }

    // Otherwise append into existing buffer
    size_t pending = c->out_len - c->out_off;
    if (pending + len > BUF_CAP) return false;

    // compact to front if needed
    if (c->out_off > 0 && pending > 0) {
        memmove(c->out, c->out + c->out_off, pending);
    }
    c->out_off = 0;
    c->out_len = pending;

    memcpy(c->out + c->out_len, data, len);
    c->out_len += len;

    uint32_t ev = EPOLLIN | EPOLLOUT | EPOLLRDHUP | EPOLLHUP | EPOLLERR;
    (void)mod_epoll(c->server->epfd, c->fd, ev, c);

    return true;
}

bool tcp_conn_write_str(TCPConn *c, const char *s) {
    if (!s) return false;
    return tcp_conn_write(c, s, strlen(s));
}

void tcp_conn_close_after_write(TCPConn *c) {
    if (!c) return;
    c->close_after_write = true;

    if (c->out_len == c->out_off) {
        c->close_now = true;
        shutdown(c->fd, SHUT_RDWR);
    }
}

void tcp_conn_close_now(TCPConn *c) {
    if (!c) return;
    c->close_now = true;
    c->close_after_write = false;
    c->out_len = c->out_off = 0;
    shutdown(c->fd, SHUT_RDWR);
}

void tcp_conn_set_user(TCPConn *c, void *user) {
    if (!c) return;
    c->user = user;
}

void *tcp_conn_get_user(TCPConn *c) {
    if (!c) return NULL;
    return c->user;
}

const char *tcp_conn_ip(const TCPConn *c) {
    if (!c) return "";
    return c->ip;
}

uint16_t tcp_conn_port(const TCPConn *c) {
    if (!c) return 0;
    return c->port;
}


//   
