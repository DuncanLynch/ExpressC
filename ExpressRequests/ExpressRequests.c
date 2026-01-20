#include "ExpressRequests.h"

#include "../ExpressTypes/ExpressHttp.h"

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static int ascii_strcasecmp(const char *a, const char *b) {
  unsigned char ca, cb;
  while (*a && *b) {
    ca = (unsigned char)tolower((unsigned char)*a++);
    cb = (unsigned char)tolower((unsigned char)*b++);
    if (ca != cb) return (int)ca - (int)cb;
  }
  return (int)tolower((unsigned char)*a) - (int)tolower((unsigned char)*b);
}

static const char *find_header_value(const ExpressHeader *h, const char *key) {
  for (; h; h = h->next) {
    if (h->key && h->value && ascii_strcasecmp(h->key, key) == 0) return h->value;
  }
  return NULL;
}

static int has_header(const ExpressHeader *h, const char *key) {
  return find_header_value(h, key) != NULL;
}

static ExpressHeader *append_header(ExpressHeader **head,
                                    const char *key,
                                    const char *value) {
  ExpressHeader *n = (ExpressHeader *)malloc(sizeof(ExpressHeader));
  if (!n) return NULL;
  n->key = strdup(key ? key : "");
  n->value = strdup(value ? value : "");
  n->next = NULL;
  if (!n->key || !n->value) {
    free(n->key);
    free(n->value);
    free(n);
    return NULL;
  }
  if (!*head) {
    *head = n;
  } else {
    ExpressHeader *cur = *head;
    while (cur->next) cur = cur->next;
    cur->next = n;
  }
  return n;
}

static ExpressStatus init_response_fields(ExpressResponse *res) {
  if (!res) return EXPRESS_PARSE_REQUEST_ERROR;
  memset(res, 0, sizeof(*res));
  res->statusCode = 0;
  return EXPRESS_OK;
}

static void free_response_fields_local(ExpressResponse *res) {
  if (!res) return;
  free(res->method);
  free(res->url);
  free(res->statusMessage);
  if (res->headers) free_headers(res->headers);
  free(res->body);
  memset(res, 0, sizeof(*res));
}

static int send_all(int sockfd, const unsigned char *buf, size_t len) {
  size_t off = 0;
  while (off < len) {
    ssize_t n = send(sockfd, buf + off, len - off, 0);
    if (n < 0) {
      if (errno == EINTR) continue;
      return -1;
    }
    if (n == 0) return -1;
    off += (size_t)n;
  }
  return 0;
}

static int parse_url_host_port_path(const ExpressRequest *req,
                                    char **out_host,
                                    char **out_port,
                                    char **out_path) {
  *out_host = NULL;
  *out_port = NULL;
  *out_path = NULL;

  const char *url = req && req->url ? req->url : "/";
  const char *p = url;

  if (strncmp(p, "https://", 8) == 0) return -2;

  if (strncmp(p, "http://", 7) == 0) {
    p += 7;
    const char *host_begin = p;
    while (*p && *p != '/' && *p != '?' && *p != '#') p++;
    const char *host_end = p;

    const char *colon = memchr(host_begin, ':', (size_t)(host_end - host_begin));
    if (colon) {
      *out_host = strndup(host_begin, (size_t)(colon - host_begin));
      *out_port = strndup(colon + 1, (size_t)(host_end - (colon + 1)));
    } else {
      *out_host = strndup(host_begin, (size_t)(host_end - host_begin));
      *out_port = strdup("80");
    }
    if (!*out_host || !*out_port) return -1;

    *out_path = (*p) ? strdup(p) : strdup("/");
    if (!*out_path) return -1;
    return 0;
  }

  const char *hosthdr = find_header_value(req->headers, "Host");
  if (!hosthdr) return -3;

  while (*hosthdr == ' ' || *hosthdr == '\t') hosthdr++;
  const char *c = strchr(hosthdr, ':');
  if (c) {
    *out_host = strndup(hosthdr, (size_t)(c - hosthdr));
    *out_port = strdup(c + 1);
  } else {
    *out_host = strdup(hosthdr);
    *out_port = strdup("80");
  }
  if (!*out_host || !*out_port) return -1;

  *out_path = strdup(url[0] ? url : "/");
  if (!*out_path) return -1;
  return 0;
}

static int decode_chunked(const unsigned char *in, size_t in_len,
                          unsigned char **out, size_t *out_len) {
  *out = NULL;
  *out_len = 0;

  size_t cap = 0;
  unsigned char *buf = NULL;

  size_t i = 0;
  while (i < in_len) {
    size_t line_start = i;
    while (i + 1 < in_len && !(in[i] == '\r' && in[i + 1] == '\n')) i++;
    if (i + 1 >= in_len) { free(buf); return -1; }

    char tmp[64];
    size_t line_len = i - line_start;
    if (line_len >= sizeof(tmp)) { free(buf); return -1; }
    memcpy(tmp, in + line_start, line_len);
    tmp[line_len] = '\0';

    char *semi = strchr(tmp, ';');
    if (semi) *semi = '\0';

    errno = 0;
    unsigned long chunk_sz = strtoul(tmp, NULL, 16);
    if (errno != 0) { free(buf); return -1; }

    i += 2;

    if (chunk_sz == 0) {
      *out = buf ? buf : (unsigned char *)calloc(1, 1);
      return (*out != NULL) ? 0 : -1;
    }

    if (i + chunk_sz + 2 > in_len) { free(buf); return -1; }

    if (*out_len + (size_t)chunk_sz + 1 > cap) {
      size_t newcap = cap ? cap : 1024;
      while (newcap < *out_len + (size_t)chunk_sz + 1) newcap *= 2;
      unsigned char *nb = (unsigned char *)realloc(buf, newcap);
      if (!nb) { free(buf); return -1; }
      buf = nb;
      cap = newcap;
    }

    memcpy(buf + *out_len, in + i, (size_t)chunk_sz);
    *out_len += (size_t)chunk_sz;
    i += (size_t)chunk_sz;

    if (!(in[i] == '\r' && in[i + 1] == '\n')) { free(buf); return -1; }
    i += 2;
  }

  free(buf);
  return -1;
}

static ExpressStatus parse_status_line(const char *line, int *out_code, char **out_msg) {
  if (!line || !out_code || !out_msg) return EXPRESS_PARSE_REQUEST_ERROR;

  const char *sp1 = strchr(line, ' ');
  if (!sp1) return EXPRESS_PARSE_REQUEST_ERROR;
  const char *sp2 = strchr(sp1 + 1, ' ');
  if (!sp2) return EXPRESS_PARSE_REQUEST_ERROR;

  char codebuf[4] = {0};
  size_t clen = (size_t)(sp2 - (sp1 + 1));
  if (clen != 3) return EXPRESS_PARSE_REQUEST_ERROR;
  memcpy(codebuf, sp1 + 1, 3);

  char *endp = NULL;
  long code = strtol(codebuf, &endp, 10);
  if (!endp || *endp != '\0') return EXPRESS_PARSE_REQUEST_ERROR;

  *out_code = (int)code;

  const char *msg = sp2 + 1;
  *out_msg = strdup(msg);
  return (*out_msg != NULL) ? EXPRESS_OK : EXPRESS_PARSE_MEM_ERR;
}

static ExpressStatus parse_headers_block(const char *headers, size_t len, ExpressHeader **out) {
  *out = NULL;

  char *copy = (char *)malloc(len + 1);
  if (!copy) return EXPRESS_PARSE_MEM_ERR;
  memcpy(copy, headers, len);
  copy[len] = '\0';

  ExpressHeader *head = NULL;

  char *save = NULL;
  for (char *line = strtok_r(copy, "\r\n", &save); line; line = strtok_r(NULL, "\r\n", &save)) {
    if (*line == '\0') continue;
    char *colon = strchr(line, ':');
    if (!colon) continue;

    *colon = '\0';
    char *key = line;
    char *val = colon + 1;
    while (*val == ' ' || *val == '\t') val++;

    if (!append_header(&head, key, val)) {
      free(copy);
      if (head) free_headers(head);
      return EXPRESS_PARSE_MEM_ERR;
    }
  }

  free(copy);
  *out = head;
  return EXPRESS_OK;
}

ExpressStatus send_req(ExpressRequest *req, ExpressResponse *res) {
  ExpressStatus st = init_response_fields(res);
  if (st != EXPRESS_OK) return st;
  if (!req) return EXPRESS_PARSE_REQUEST_ERROR;

  res->method = req->method ? strdup(req->method) : NULL;
  res->url = req->url ? strdup(req->url) : NULL;
  res->timeout_ms = req->timeout_ms;

  char *host = NULL, *port = NULL, *path = NULL;
  int u = parse_url_host_port_path(req, &host, &port, &path);
  if (u == -2) { free_response_fields_local(res); return EXPRESS_PARSE_REQUEST_ERROR; }
  if (u != 0)  { free_response_fields_local(res); return EXPRESS_PARSE_REQUEST_ERROR; }

  unsigned char outbuf[MAX_BUFFER_SIZE];
  size_t off = 0;

  const char *method = req->method ? req->method : "GET";
  const char *httpv  = req->httpVersion ? req->httpVersion : "HTTP/1.1";
  const char *body   = req->body;
  size_t body_len = (body && req->bodyLength) ? req->bodyLength :
                    (body ? strlen(body) : 0);

  int n = snprintf((char *)outbuf + off, MAX_BUFFER_SIZE - off,
                   "%s %s %s\r\n", method, path, httpv);
  if (n < 0 || (size_t)n >= MAX_BUFFER_SIZE - off) {
    free(host); free(port); free(path);
    free_response_fields_local(res);
    return EXPRESS_PARSE_REQUEST_ERROR;
  }
  off += (size_t)n;

  if (!has_header(req->headers, "Host")) {
    n = snprintf((char *)outbuf + off, MAX_BUFFER_SIZE - off,
                 "Host: %s\r\n", host);
    if (n < 0 || (size_t)n >= MAX_BUFFER_SIZE - off) {
      free(host); free(port); free(path);
      free_response_fields_local(res);
      return EXPRESS_PARSE_REQUEST_ERROR;
    }
    off += (size_t)n;
  }

  for (ExpressHeader *h = req->headers; h; h = h->next) {
    if (!h->key || !h->value) continue;
    n = snprintf((char *)outbuf + off, MAX_BUFFER_SIZE - off,
                 "%s: %s\r\n", h->key, h->value);
    if (n < 0 || (size_t)n >= MAX_BUFFER_SIZE - off) {
      free(host); free(port); free(path);
      free_response_fields_local(res);
      return EXPRESS_PARSE_REQUEST_ERROR;
    }
    off += (size_t)n;
  }

  if (body && body_len > 0 && !has_header(req->headers, "Content-Length")) {
    n = snprintf((char *)outbuf + off, MAX_BUFFER_SIZE - off,
                 "Content-Length: %zu\r\n", body_len);
    if (n < 0 || (size_t)n >= MAX_BUFFER_SIZE - off) {
      free(host); free(port); free(path);
      free_response_fields_local(res);
      return EXPRESS_PARSE_REQUEST_ERROR;
    }
    off += (size_t)n;
  }

  n = snprintf((char *)outbuf + off, MAX_BUFFER_SIZE - off, "\r\n");
  if (n < 0 || (size_t)n >= MAX_BUFFER_SIZE - off) {
    free(host); free(port); free(path);
    free_response_fields_local(res);
    return EXPRESS_PARSE_REQUEST_ERROR;
  }
  off += (size_t)n;

  if (body && body_len > 0) {
    if (body_len > MAX_BUFFER_SIZE - off) {
      free(host); free(port); free(path);
      free_response_fields_local(res);
      return EXPRESS_PARSE_REQUEST_ERROR;
    }
    memcpy(outbuf + off, body, body_len);
    off += body_len;
  }

  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  struct addrinfo *ai = NULL;
  int gai = getaddrinfo(host, port, &hints, &ai);
  if (gai != 0) {
    free(host); free(port); free(path);
    free_response_fields_local(res);
    return EXPRESS_PARSE_REQUEST_ERROR;
  }

  int sockfd = -1;
  for (struct addrinfo *p = ai; p; p = p->ai_next) {
    sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (sockfd < 0) continue;
    if (connect(sockfd, p->ai_addr, p->ai_addrlen) == 0) break;
    close(sockfd);
    sockfd = -1;
  }
  freeaddrinfo(ai);

  if (sockfd < 0) {
    free(host); free(port); free(path);
    free_response_fields_local(res);
    return EXPRESS_PARSE_REQUEST_ERROR;
  }

  if (send_all(sockfd, outbuf, off) != 0) {
    close(sockfd);
    free(host); free(port); free(path);
    free_response_fields_local(res);
    return EXPRESS_PARSE_REQUEST_ERROR;
  }

  size_t rcap = 8192, rlen = 0;
  unsigned char *rbuf = (unsigned char *)malloc(rcap);
  if (!rbuf) {
    close(sockfd);
    free(host); free(port); free(path);
    free_response_fields_local(res);
    return EXPRESS_PARSE_MEM_ERR;
  }

  for (;;) {
    if (rlen == rcap) {
      size_t newcap = rcap * 2;
      unsigned char *nb = (unsigned char *)realloc(rbuf, newcap);
      if (!nb) {
        free(rbuf);
        close(sockfd);
        free(host); free(port); free(path);
        free_response_fields_local(res);
        return EXPRESS_PARSE_MEM_ERR;
      }
      rbuf = nb;
      rcap = newcap;
    }
    ssize_t nr = recv(sockfd, rbuf + rlen, rcap - rlen, 0);
    if (nr < 0) {
      if (errno == EINTR) continue;
      free(rbuf);
      close(sockfd);
      free(host); free(port); free(path);
      free_response_fields_local(res);
      return EXPRESS_PARSE_REQUEST_ERROR;
    }
    if (nr == 0) break;
    rlen += (size_t)nr;
  }
  close(sockfd);
  free(host); free(port); free(path);

  size_t hdr_end = 0;
  int found = 0;
  for (size_t i = 0; i + 3 < rlen; i++) {
    if (rbuf[i] == '\r' && rbuf[i + 1] == '\n' && rbuf[i + 2] == '\r' && rbuf[i + 3] == '\n') {
      hdr_end = i + 4;
      found = 1;
      break;
    }
  }
  if (!found) {
    free(rbuf);
    free_response_fields_local(res);
    return EXPRESS_PARSE_REQUEST_ERROR;
  }

  char *hdr_copy = (char *)malloc(hdr_end + 1);
  if (!hdr_copy) {
    free(rbuf);
    free_response_fields_local(res);
    return EXPRESS_PARSE_MEM_ERR;
  }
  memcpy(hdr_copy, rbuf, hdr_end);
  hdr_copy[hdr_end] = '\0';

  char *line_end = strstr(hdr_copy, "\r\n");
  if (!line_end) {
    free(hdr_copy);
    free(rbuf);
    free_response_fields_local(res);
    return EXPRESS_PARSE_REQUEST_ERROR;
  }
  *line_end = '\0';

  st = parse_status_line(hdr_copy, &res->statusCode, &res->statusMessage);
  if (st != EXPRESS_OK) {
    free(hdr_copy);
    free(rbuf);
    free_response_fields_local(res);
    return st;
  }

  const char *headers_block = line_end + 2;
  size_t headers_block_len = (size_t)((hdr_copy + hdr_end) - headers_block);
  size_t raw_headers_len = hdr_end - ((size_t)(line_end - hdr_copy) + 2);

  st = parse_headers_block(headers_block, raw_headers_len, &res->headers);
  if (st != EXPRESS_OK) {
    free(hdr_copy);
    free(rbuf);
    free_response_fields_local(res);
    return st;
  }

  free(hdr_copy);

  const unsigned char *body_start = rbuf + hdr_end;
  size_t body_avail = rlen - hdr_end;

  const char *te = find_header_value(res->headers, "Transfer-Encoding");
  const char *cl = find_header_value(res->headers, "Content-Length");

  if (te && ascii_strcasecmp(te, "chunked") == 0) {
    unsigned char *decoded = NULL;
    size_t decoded_len = 0;
    if (decode_chunked(body_start, body_avail, &decoded, &decoded_len) != 0) {
      free(rbuf);
      free_response_fields_local(res);
      return EXPRESS_PARSE_REQUEST_ERROR;
    }
    res->body = (char *)malloc(decoded_len + 1);
    if (!res->body) {
      free(decoded);
      free(rbuf);
      free_response_fields_local(res);
      return EXPRESS_PARSE_MEM_ERR;
    }
    memcpy(res->body, decoded, decoded_len);
    res->body[decoded_len] = '\0';
    res->bodyLength = decoded_len;
    free(decoded);
  } else if (cl) {
    char *endp = NULL;
    long want = strtol(cl, &endp, 10);
    if (!endp || (*endp != '\0' && !isspace((unsigned char)*endp)) || want < 0) {
      free(rbuf);
      free_response_fields_local(res);
      return EXPRESS_PARSE_REQUEST_ERROR;
    }
    size_t want_sz = (size_t)want;
    if (want_sz > body_avail) {
      free(rbuf);
      free_response_fields_local(res);
      return EXPRESS_PARSE_REQUEST_ERROR;
    }
    res->body = (char *)malloc(want_sz + 1);
    if (!res->body) {
      free(rbuf);
      free_response_fields_local(res);
      return EXPRESS_PARSE_MEM_ERR;
    }
    memcpy(res->body, body_start, want_sz);
    res->body[want_sz] = '\0';
    res->bodyLength = want_sz;
  } else {
    res->body = (char *)malloc(body_avail + 1);
    if (!res->body) {
      free(rbuf);
      free_response_fields_local(res);
      return EXPRESS_PARSE_MEM_ERR;
    }
    memcpy(res->body, body_start, body_avail);
    res->body[body_avail] = '\0';
    res->bodyLength = body_avail;
  }

  free(rbuf);
  return EXPRESS_OK;
}

