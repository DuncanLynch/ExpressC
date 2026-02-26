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

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    return 2;
  }
  uint16_t port = (uint16_t)strtoul(argv[1], NULL, 10);

  // Avoid SIGPIPE terminating process on send() to a dead peer.
  signal(SIGPIPE, SIG_IGN);

  int listen_fd = create_listen_socket(port);
  if (listen_fd == -1) die("create_listen_socket");

  if (set_nonblocking(listen_fd) == -1) die("set_nonblocking(listen_fd)");

  int epfd = epoll_create1(EPOLL_CLOEXEC);
  if (epfd == -1) die("epoll_create1");

  // Put listen socket into epoll
  struct epoll_event ev;
  memset(&ev, 0, sizeof(ev));
  ev.events = EPOLLIN;
  ev.data.fd = listen_fd;
  if (epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &ev) == -1) die("epoll_ctl ADD listen");

  fprintf(stderr, "listening on 0.0.0.0:%u\n", port);

  struct epoll_event events[MAX_EVENTS];

  for (;;) {
    int n = epoll_wait(epfd, events, MAX_EVENTS, -1);
    if (n == -1) {
      if (errno == EINTR) continue;
      die("epoll_wait");
    }

    for (int i = 0; i < n; i++) {
      uint32_t e = events[i].events;

      // Listener event uses data.fd; clients use data.ptr
      if (events[i].data.fd == listen_fd) {
        accept_loop(epfd, listen_fd);
        continue;
      }

      struct conn *c = (struct conn *)events[i].data.ptr;

      if (e & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
        conn_close(epfd, c);
        continue;
      }

      if (e & EPOLLIN) {
        if (!handle_read(epfd, c)) {
          conn_close(epfd, c);
          continue;
        }
      }

      if (e & EPOLLOUT) {
        if (!handle_write(epfd, c)) {
          conn_close(epfd, c);
          continue;
        }
      }
    }
  }

  // unreachable in this example
  close(epfd);
  close(listen_fd);
  return 0;
}