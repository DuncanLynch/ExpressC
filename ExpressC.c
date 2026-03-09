#include "ExpressC.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "TCPServer/TCPServer.h"
#include "types.h"

#define MAX_METHODS 6
#define MAX_ROUTES 512

struct HTTPConn {
    byte* bytes;
    size_t bytes_len;
    size_t bytes_off;
    size_t bytes_cap;
    bool parsed_headers;
    bool sent_continue;

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
    TCPServer* tcp_server;

    size_t total_requests;

} ExpressServer;

static int caseless_stricmp(const char* lhs, const char* rhs) {
    if (lhs == NULL || rhs == NULL) return lhs != rhs;

    while (*lhs != '\0' && *rhs != '\0') {
        int l = tolower((unsigned char)*lhs);
        int r = tolower((unsigned char)*rhs);
        if (l != r) return l - r;
        lhs++;
        rhs++;
    }

    return tolower((unsigned char)*lhs) - tolower((unsigned char)*rhs);
}

static int from_hex_digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';

    c = (char)tolower((unsigned char)c);
    if (c >= 'a' && c <= 'f') return (c - 'a') + 10;

    return -1;
}

static char url_decode_char(const char* encoded) {
    if (encoded == NULL || encoded[0] == '\0') return '\0';
    if (encoded[0] == '+') return ' ';
    if (encoded[0] != '%') return encoded[0];
    if (encoded[1] == '\0' || encoded[2] == '\0') return '\0';

    int hi = from_hex_digit(encoded[1]);
    int lo = from_hex_digit(encoded[2]);
    if (hi < 0 || lo < 0) return '\0';

    return (char)((hi << 4) | lo);
}

static char* skip_ascii_whitespace(char* s) {
    if (s == NULL) return NULL;

    while (*s != '\0' && isspace((unsigned char)*s)) s++;
    return s;
}

static const char* skip_const_ascii_whitespace(const char* s,
                                               const char* end) {
    if (s == NULL) return NULL;

    while (s < end && isspace((unsigned char)*s)) s++;
    return s;
}

static const char* find_ascii_whitespace(const char* s, const char* end) {
    if (s == NULL) return NULL;

    while (s < end && !isspace((unsigned char)*s)) s++;
    return s;
}

static void trim_ascii_trailing_whitespace(char* s) {
    if (s == NULL) return;

    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        s[len - 1] = '\0';
        len--;
    }
}

static char* clone_trimmed_range(const char* start, const char* end) {
    if (start == NULL || end == NULL || start > end) return NULL;

    while (start < end && isspace((unsigned char)*start)) start++;
    while (end > start && isspace((unsigned char)*(end - 1))) end--;

    size_t len = (size_t)(end - start);
    char* copy = (char*)malloc(len + 1);
    if (copy == NULL) return NULL;

    if (len > 0) memcpy(copy, start, len);
    copy[len] = '\0';
    return copy;
}

static bool parse_content_length_value(const char* value, size_t* out) {
    if (value == NULL || out == NULL || *value == '\0') return false;

    size_t result = 0;
    for (const unsigned char* p = (const unsigned char*)value; *p != '\0'; p++) {
        if (!isdigit(*p)) return false;
        if (result > (SIZE_MAX - (size_t)(*p - '0')) / 10) return false;
        result = (result * 10) + (size_t)(*p - '0');
    }

    *out = result;
    return true;
}

static bool parse_http_status_code(const char* status_code,
                                   uint16_t* out_status_code) {
    if (status_code == NULL || status_code[0] == '\0' ||
        status_code[1] == '\0' || status_code[2] == '\0' ||
        status_code[3] != '\0') {
        return false;
    }

    uint16_t code = 0;
    for (size_t i = 0; i < 3; i++) {
        unsigned char digit = (unsigned char)status_code[i];
        if (!isdigit(digit)) return false;
        code = (uint16_t)((code * 10) + (uint16_t)(digit - '0'));
    }

    if (code < 100 || code > 599) return false;

    if (out_status_code != NULL) *out_status_code = code;
    return true;
}

static bool caseless_token_equals(const char* start, const char* end,
                                  const char* token) {
    if (start == NULL || end == NULL || token == NULL || start > end) {
        return false;
    }

    size_t len = (size_t)(end - start);
    size_t token_len = strlen(token);
    if (len != token_len) return false;

    for (size_t i = 0; i < len; i++) {
        int lhs = tolower((unsigned char)start[i]);
        int rhs = tolower((unsigned char)token[i]);
        if (lhs != rhs) return false;
    }

    return true;
}

static bool header_value_has_token(const char* value, const char* token) {
    if (value == NULL || token == NULL) return false;

    const char* cursor = value;
    while (*cursor != '\0') {
        while (*cursor != '\0' &&
               (isspace((unsigned char)*cursor) || *cursor == ',')) {
            cursor++;
        }

        const char* token_start = cursor;
        while (*cursor != '\0' && *cursor != ',') cursor++;

        const char* token_end = cursor;
        while (token_end > token_start &&
               isspace((unsigned char)*(token_end - 1))) {
            token_end--;
        }

        if (caseless_token_equals(token_start, token_end, token)) return true;
    }

    return false;
}

static bool header_value_has_only_token(const char* value, const char* token) {
    if (value == NULL || token == NULL) return false;

    bool saw_token = false;
    const char* cursor = value;
    while (*cursor != '\0') {
        while (*cursor != '\0' &&
               (isspace((unsigned char)*cursor) || *cursor == ',')) {
            cursor++;
        }
        if (*cursor == '\0') break;

        const char* token_start = cursor;
        while (*cursor != '\0' && *cursor != ',') cursor++;

        const char* token_end = cursor;
        while (token_end > token_start &&
               isspace((unsigned char)*(token_end - 1))) {
            token_end--;
        }

        if (!caseless_token_equals(token_start, token_end, token)) return false;
        saw_token = true;
    }

    return saw_token;
}
static bool ensure_http_conn_cap(struct HTTPConn* conn, size_t need) {
    if (conn == NULL) return false;
    if (need <= conn->bytes_cap) return true;

    size_t new_cap = conn->bytes_cap ? conn->bytes_cap : 1024;
    while (new_cap < need) {
        if (new_cap > SIZE_MAX / 2) return false;
        new_cap *= 2;
    }

    byte* next = (byte*)realloc(conn->bytes, new_cap);
    if (next == NULL) return false;

    conn->bytes = next;
    conn->bytes_cap = new_cap;
    return true;
}

static void http_request_cleanup(http_request* req) {
    if (req == NULL) return;

    for (size_t i = 0; i < req->request_params_len; i++) {
        free(req->request_params[i].key);
        free(req->request_params[i].value);
    }

    for (size_t i = 0; i < req->route_params_len; i++) {
        free(req->route_params[i].key);
        free(req->route_params[i].value);
    }

    for (size_t i = 0; i < req->headers_len; i++) {
        free(req->headers[i].key);
        free(req->headers[i].value);
    }

    free(req);
}

static void http_conn_clear_request(struct HTTPConn* conn) {
    if (conn == NULL) return;

    http_request_cleanup(conn->req);
    conn->req = NULL;
    conn->parsed_headers = false;
    conn->sent_continue = false;
    conn->bytes_off = 0;
}

static void http_conn_reset(struct HTTPConn* conn) {
    if (conn == NULL) return;

    http_conn_clear_request(conn);
    free(conn->bytes);

    conn->bytes = NULL;
    conn->bytes_len = 0;
    conn->bytes_cap = 0;
}

static void http_conn_consume_bytes(struct HTTPConn* conn, size_t consumed) {
    if (conn == NULL) return;

    if (consumed >= conn->bytes_len) {
        conn->bytes_len = 0;
        if (conn->bytes != NULL) conn->bytes[0] = '\0';
        return;
    }

    size_t remaining = conn->bytes_len - consumed;
    memmove(conn->bytes, conn->bytes + consumed, remaining);
    conn->bytes_len = remaining;
    conn->bytes[remaining] = '\0';
}

static void http_conn_destroy(struct HTTPConn* conn) {
    if (conn == NULL) return;

    http_conn_reset(conn);
    free(conn);
}

static http_response response_default(void) {
    http_response res;
    memset(&res, 0, sizeof(res));
    res.status_code = "200";
    return res;
}

static const char* validated_response_status_code(const http_response* res) {
    const char* status_code =
        (res != NULL && res->status_code != NULL) ? res->status_code : "200";

    if (!parse_http_status_code(status_code, NULL)) return "500";
    return status_code;
}

void log_response(TCPConn* conn, const http_request* req, const http_response* res) {
    const char* ip = tcp_conn_ip(conn);
    const char* status_code = validated_response_status_code(res);

    printf("%s \"%s %s\" %s %zu\n",
           ip ? ip : "-",
           req->method,
           req->route,
           status_code,
           res->content_length);
}

static void response_set_static(http_response* res, const char* status_code,
                                const char* body) {
    if (res == NULL) return;

    res->status_code = (char*)status_code;
    res->body = (byte*)body;
    res->content_length = body == NULL ? 0 : strlen(body);
}

static const char* reason_phrase(const char* status_code) {
    uint16_t code = 0;
    if (!parse_http_status_code(status_code, &code)) return "Internal Server Error";

    switch (code) {
        case 100:
            return "Continue";
        case 101:
            return "Switching Protocols";
        case 102:
            return "Processing";
        case 103:
            return "Early Hints";
        case 104:
            return "Upload Resumption Supported";
        case 200:
            return "OK";
        case 201:
            return "Created";
        case 202:
            return "Accepted";
        case 203:
            return "Non-Authoritative Information";
        case 204:
            return "No Content";
        case 205:
            return "Reset Content";
        case 206:
            return "Partial Content";
        case 207:
            return "Multi-Status";
        case 208:
            return "Already Reported";
        case 226:
            return "IM Used";
        case 300:
            return "Multiple Choices";
        case 301:
            return "Moved Permanently";
        case 302:
            return "Found";
        case 303:
            return "See Other";
        case 304:
            return "Not Modified";
        case 305:
            return "Use Proxy";
        case 306:
            return "(Unused)";
        case 307:
            return "Temporary Redirect";
        case 308:
            return "Permanent Redirect";
        case 400:
            return "Bad Request";
        case 401:
            return "Unauthorized";
        case 402:
            return "Payment Required";
        case 403:
            return "Forbidden";
        case 404:
            return "Not Found";
        case 405:
            return "Method Not Allowed";
        case 406:
            return "Not Acceptable";
        case 407:
            return "Proxy Authentication Required";
        case 408:
            return "Request Timeout";
        case 409:
            return "Conflict";
        case 410:
            return "Gone";
        case 411:
            return "Length Required";
        case 412:
            return "Precondition Failed";
        case 413:
            return "Content Too Large";
        case 414:
            return "URI Too Long";
        case 415:
            return "Unsupported Media Type";
        case 416:
            return "Range Not Satisfiable";
        case 417:
            return "Expectation Failed";
        case 418:
            return "(Unused)";
        case 421:
            return "Misdirected Request";
        case 422:
            return "Unprocessable Content";
        case 423:
            return "Locked";
        case 424:
            return "Failed Dependency";
        case 425:
            return "Too Early";
        case 426:
            return "Upgrade Required";
        case 428:
            return "Precondition Required";
        case 429:
            return "Too Many Requests";
        case 431:
            return "Request Header Fields Too Large";
        case 451:
            return "Unavailable For Legal Reasons";
        case 500:
            return "Internal Server Error";
        case 501:
            return "Not Implemented";
        case 502:
            return "Bad Gateway";
        case 503:
            return "Service Unavailable";
        case 504:
            return "Gateway Timeout";
        case 505:
            return "HTTP Version Not Supported";
        case 506:
            return "Variant Also Negotiates";
        case 507:
            return "Insufficient Storage";
        case 508:
            return "Loop Detected";
        case 510:
            return "Not Extended";
        case 511:
            return "Network Authentication Required";
        default:
            return "";
    }
}

static bool parse_request_line(http_request* req, const char* request_line) {
    if (req == NULL || request_line == NULL) return false;

    const char* line_end = strstr(request_line, "\r\n");
    if (line_end == NULL) return false;

    const char* cursor = skip_const_ascii_whitespace(request_line, line_end);
    const char* method_end = find_ascii_whitespace(cursor, line_end);
    if (cursor == method_end || method_end == NULL) return false;

    size_t method_len = (size_t)(method_end - cursor);
    if (method_len >= sizeof(req->method)) return false;
    memcpy(req->method, cursor, method_len);
    req->method[method_len] = '\0';

    cursor = skip_const_ascii_whitespace(method_end, line_end);
    const char* route_end = find_ascii_whitespace(cursor, line_end);
    if (cursor == route_end || route_end == NULL) return false;

    size_t route_len = (size_t)(route_end - cursor);
    if (route_len >= sizeof(req->route)) return false;
    memcpy(req->route, cursor, route_len);
    req->route[route_len] = '\0';

    cursor = skip_const_ascii_whitespace(route_end, line_end);
    const char* version_end = find_ascii_whitespace(cursor, line_end);
    if (cursor == version_end || version_end == NULL) return false;

    size_t version_len = (size_t)(version_end - cursor);
    if (version_len >= sizeof(req->version)) return false;
    memcpy(req->version, cursor, version_len);
    req->version[version_len] = '\0';

    cursor = skip_const_ascii_whitespace(version_end, line_end);
    return cursor == line_end;
}

static int32_t parse_headers(const struct HTTPConn* c, http_request* req) {
    if (c == NULL || req == NULL || c->bytes == NULL) return -3;

    char* headers_end = strstr((char*)c->bytes, "\r\n\r\n");
    if (headers_end == NULL) return -1;

    size_t header_bytes = (size_t)(headers_end - (char*)c->bytes) + 4;
    if (header_bytes > INT32_MAX) return -3;

    req->headers_len = 0;
    req->host = NULL;
    req->content_length = 0;
    req->content_type = NULL;
    req->body = NULL;

    size_t host_count = 0;

    if (!parse_request_line(req, (char*)c->bytes)) return -2;

    const char* line = strstr((char*)c->bytes, "\r\n");
    if (line == NULL) return -2;
    line += 2;

    while (line < headers_end) {
        const char* line_end = strstr(line, "\r\n");
        if (line_end == NULL || line_end > headers_end) return -3;

        const char* colon = line;
        while (colon < line_end && *colon != ':') colon++;
        if (colon == line_end) return -3;
        if (req->headers_len >= MAX_HEADERS) return -4;

        char* key = clone_trimmed_range(line, colon);
        char* value = clone_trimmed_range(colon + 1, line_end);
        if (key == NULL || value == NULL) {
            free(key);
            free(value);
            return -6;
        }
        if (*key == '\0') {
            free(key);
            free(value);
            return -3;
        }

        req->headers[req->headers_len].key = key;
        req->headers[req->headers_len].value = value;
        req->headers_len++;

        if (caseless_stricmp(key, "Host") == 0) {
            host_count++;
            req->host = value;
            if (*value == '\0' || host_count > 1) return -3;
        } else if (caseless_stricmp(key, "Content-Type") == 0) {
            req->content_type = value;
        } else if (caseless_stricmp(key, "Content-Length") == 0) {
            size_t content_length = 0;
            if (!parse_content_length_value(value, &content_length)) return -5;
            req->content_length = content_length;
        }

        line = line_end + 2;
    }

    if (strcmp(req->version, "HTTP/1.1") == 0 && host_count != 1) return -3;

    return (int32_t)header_bytes;
}
static bool response_has_header(const http_response* res, const char* key) {
    if (res == NULL || key == NULL) return false;

    for (size_t i = 0; i < res->headers_len; i++) {
        if (res->headers[i].key == NULL) continue;
        if (caseless_stricmp(res->headers[i].key, key) == 0) return true;
    }

    return false;
}

static bool request_has_supported_version(const http_request* req) {
    if (req == NULL) return false;

    return strcmp(req->version, "HTTP/1.1") == 0 ||
           strcmp(req->version, "HTTP/1.0") == 0;
}

static bool request_expectation_supported(const http_request* req) {
    if (req == NULL) return false;

    header* expect = get_request_header((http_request*)req, "Expect");
    if (expect == NULL) return true;

    return strcmp(req->version, "HTTP/1.1") == 0 &&
           header_value_has_only_token(expect->value, "100-continue");
}

static bool request_expects_continue(const http_request* req) {
    if (req == NULL) return false;

    header* expect = get_request_header((http_request*)req, "Expect");
    if (expect == NULL) return false;

    return header_value_has_only_token(expect->value, "100-continue");
}

static bool request_is_head(const http_request* req) {
    return req != NULL && strcmp(req->method, "HEAD") == 0;
}

static const char* response_http_version(const http_request* req) {
    if (req != NULL && strcmp(req->version, "HTTP/1.0") == 0) {
        return "HTTP/1.0";
    }

    return "HTTP/1.1";
}

static bool response_must_not_have_body(const http_request* req,
                                        const http_response* res) {
    const char* status_code = validated_response_status_code(res);

    if (request_is_head(req)) return true;
    if (status_code[0] == '1') return true;

    return strcmp(status_code, "204") == 0 ||
           strcmp(status_code, "304") == 0;
}

static bool write_continue_response(TCPConn* c) {
    return tcp_conn_write_str(c, "HTTP/1.1 100 Continue\r\n\r\n");
}

static const char* method_name(enum Method method) {
    switch (method) {
        case GET:
            return "GET";
        case POST:
            return "POST";
        case DELETE:
            return "DELETE";
        case PUT:
            return "PUT";
        case PATCH:
            return "PATCH";
        case HEAD:
            return "HEAD";
        default:
            return NULL;
    }
}

static bool route_supports_method(const struct Route* route,
                                  enum Method method) {
    if (route == NULL || method >= MAX_METHODS) return false;

    if (method == HEAD) {
        return route->handlers[HEAD].handler != NULL ||
               route->handlers[GET].handler != NULL;
    }

    return route->handlers[method].handler != NULL;
}

static bool append_allow_method(char* buffer, size_t buffer_size, size_t* off,
                                const char* method) {
    if (buffer == NULL || off == NULL || method == NULL) return false;

    size_t method_len = strlen(method);
    size_t prefix_len = *off == 0 ? 0 : 2;
    if (*off + prefix_len + method_len + 1 > buffer_size) return false;

    if (prefix_len != 0) {
        buffer[(*off)++] = ',';
        buffer[(*off)++] = ' ';
    }

    memcpy(buffer + *off, method, method_len);
    *off += method_len;
    buffer[*off] = '\0';
    return true;
}

static bool build_allow_header_value(const struct Route* route, char* buffer,
                                     size_t buffer_size) {
    if (route == NULL || buffer == NULL || buffer_size == 0) return false;

    static const enum Method allow_order[] = {GET, HEAD, POST, PUT, DELETE,
                                              PATCH};

    size_t off = 0;
    buffer[0] = '\0';
    for (size_t i = 0; i < sizeof(allow_order) / sizeof(allow_order[0]); i++) {
        enum Method method = allow_order[i];
        if (!route_supports_method(route, method)) continue;
        if (!append_allow_method(buffer, buffer_size, &off,
                                 method_name(method))) {
            return false;
        }
    }

    return off != 0;
}

static bool request_should_keep_alive(const http_request* req) {
    if (req == NULL) return false;

    header* connection = get_request_header((http_request*)req, "Connection");
    if (strcmp(req->version, "HTTP/1.1") == 0) {
        return connection == NULL ||
               !header_value_has_token(connection->value, "close");
    }

    if (strcmp(req->version, "HTTP/1.0") == 0) {
        return connection != NULL &&
               header_value_has_token(connection->value, "keep-alive");
    }

    return false;
}

static bool response_should_close(const http_request* req,
                                  const http_response* res) {
    header* connection = get_response_header((http_response*)res, "Connection");
    if (connection != NULL && connection->value != NULL) {
        if (header_value_has_token(connection->value, "close")) return true;
        if (header_value_has_token(connection->value, "keep-alive")) return false;
    }

    return !request_should_keep_alive(req);
}

static void response_cleanup(http_response* res) {
    if (res == NULL) return;

    free(res->headers);
    res->headers = NULL;
    res->headers_len = 0;
}

param* get_request_param(http_request* req, const char* key) {
    if (req == NULL || key == NULL) return NULL;

    for (size_t i = 0; i < req->request_params_len; i++) {
        if (req->request_params[i].key == NULL) continue;
        if (caseless_stricmp(req->request_params[i].key, key) == 0) {
            return &req->request_params[i];
        }
    }

    return NULL;
}

param* get_request_route_param(http_request* req, const char* key) {
    if (req == NULL || key == NULL) return NULL;

    for (size_t i = 0; i < req->route_params_len; i++) {
        if (req->route_params[i].key == NULL) continue;
        if (caseless_stricmp(req->route_params[i].key, key) == 0) {
            return &req->route_params[i];
        }
    }

    return NULL;
}

header* get_request_header(http_request* req, const char* key) {
    if (req == NULL || key == NULL) return NULL;

    for (size_t i = 0; i < req->headers_len; i++) {
        if (req->headers[i].key == NULL) continue;
        if (caseless_stricmp(req->headers[i].key, key) == 0) {
            return &req->headers[i];
        }
    }

    return NULL;
}

byte* get_request_body(http_request* req) {
    if (req == NULL) return NULL;
    return req->body;
}

size_t get_request_body_len(http_request* req) {
    if (req == NULL) return 0;
    return req->content_length;
}

char* get_request_content_type(http_request* req) {
    if (req == NULL) return NULL;
    if (req->content_type != NULL) return req->content_type;

    header* content_type = get_request_header(req, "Content-Type");
    if (content_type == NULL) return NULL;
    return content_type->value;
}

header* get_response_header(http_response* res, const char* key) {
    if (res == NULL || key == NULL) return NULL;

    for (size_t i = 0; i < res->headers_len; i++) {
        if (res->headers[i].key == NULL) continue;
        if (caseless_stricmp(res->headers[i].key, key) == 0) {
            return &res->headers[i];
        }
    }

    return NULL;
}

bool set_response_header(http_response* res, const char* key,
                         const char* value) {
    if (res == NULL || key == NULL || value == NULL) return false;

    header* existing = get_response_header(res, key);
    if (existing != NULL) {
        existing->value = (char*)value;
        return true;
    }

    header* next =
        (header*)realloc(res->headers, (res->headers_len + 1) * sizeof(*next));
    if (next == NULL) return false;

    res->headers = next;
    res->headers[res->headers_len].key = (char*)key;
    res->headers[res->headers_len].value = (char*)value;
    res->headers_len++;
    return true;
}

bool set_response_body(http_response* res, const byte* body,
                       const size_t body_len) {
    if (res == NULL) return false;
    if (body == NULL && body_len > 0) return false;

    res->body = (byte*)body;
    res->content_length = body_len;
    return true;
}

bool set_response_status(http_response* res, const char* status) {
    if (res == NULL || !parse_http_status_code(status, NULL)) return false;

    res->status_code = (char*)status;
    return true;
}


static bool write_response(TCPConn* c, const http_request* req,
                           const http_response* res) {
    char line[1024];
    const char* status_code = validated_response_status_code(res);
    const char* reason = reason_phrase(status_code);
    const char* version = response_http_version(req);
    size_t content_length = res != NULL ? res->content_length : 0;
    bool close = response_should_close(req, res);
    bool omit_body = response_must_not_have_body(req, res);

    int len = snprintf(line, sizeof(line), "%s %s %s\r\n", version,
                       status_code, reason);
    if (len < 0 || (size_t)len >= sizeof(line)) return false;
    if (!tcp_conn_write(c, line, (size_t)len)) return false;

    if (!response_has_header(res, "Content-Length")) {
        len = snprintf(line, sizeof(line), "Content-Length: %zu\r\n",
                       content_length);
        if (len < 0 || (size_t)len >= sizeof(line)) return false;
        if (!tcp_conn_write(c, line, (size_t)len)) return false;
    }

    if (!response_has_header(res, "Connection")) {
        const char* connection_value = NULL;
        if (close) {
            connection_value = "close";
        } else if (req != NULL && strcmp(req->version, "HTTP/1.0") == 0) {
            connection_value = "keep-alive";
        }

        if (connection_value != NULL) {
            len = snprintf(line, sizeof(line), "Connection: %s\r\n",
                           connection_value);
            if (len < 0 || (size_t)len >= sizeof(line)) return false;
            if (!tcp_conn_write(c, line, (size_t)len)) return false;
        }
    }

    if (res != NULL) {
        for (size_t i = 0; i < res->headers_len; i++) {
            const char* key = res->headers[i].key;
            const char* value = res->headers[i].value;
            if (key == NULL || value == NULL) continue;

            len = snprintf(line, sizeof(line), "%s: %s\r\n", key, value);
            if (len < 0 || (size_t)len >= sizeof(line)) return false;
            if (!tcp_conn_write(c, line, (size_t)len)) return false;
        }
    }

    if (!tcp_conn_write_str(c, "\r\n")) return false;

    if (!omit_body && res != NULL && res->body != NULL &&
        res->content_length > 0) {
        if (!tcp_conn_write(c, res->body, res->content_length)) return false;
    }

    if (close) tcp_conn_close_after_write(c);
    return true;
}

enum Method get_method_from_str(char* method) {
    if (method == NULL) {
        return (enum Method)MAX_METHODS;
    }

    if (strcmp(method, "GET") == 0) {
        return GET;
    } else if (strcmp(method, "POST") == 0) {
        return POST;
    } else if (strcmp(method, "DELETE") == 0) {
        return DELETE;
    } else if (strcmp(method, "PUT") == 0) {
        return PUT;
    } else if (strcmp(method, "PATCH") == 0) {
        return PATCH;
    } else if (strcmp(method, "HEAD") == 0) {
        return HEAD;
    }

    return (enum Method)MAX_METHODS;
}

struct Route* find_route(ExpressRouter* r, char* route) {
    if (r == NULL || route == NULL) return NULL;

    for (size_t i = 0; i < r->routes_off; i++) {
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

int32_t router_add(ExpressRouter* r, char* route, enum Method method,
                   route_handler routing_func) {
    if (r == NULL || route == NULL || routing_func == NULL) return -1;
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

void on_accept(void* ctx, TCPConn* c) {
    (void)ctx;

    struct HTTPConn* conn = (struct HTTPConn*)calloc(1, sizeof(*conn));
    if (conn == NULL) {
        tcp_conn_close_now(c);
        return;
    }
    conn->parsed_headers = false;

    tcp_conn_set_user(c, conn);
}

void on_close(void* ctx, TCPConn* c) {
    (void)ctx;

    struct HTTPConn* conn = (struct HTTPConn*)tcp_conn_get_user(c);
    http_conn_destroy(conn);
}

void on_bytes(void* ctx, TCPConn* c, const byte* bytes, size_t len) {
    struct HTTPConn* conn = (struct HTTPConn*)tcp_conn_get_user(c);
    ExpressServer* s = (ExpressServer*)ctx;

    if (conn == NULL || s == NULL) {
        tcp_conn_close_now(c);
        return;
    }

    if (len > SIZE_MAX - conn->bytes_len - 1 ||
        !ensure_http_conn_cap(conn, conn->bytes_len + len + 1)) {
        tcp_conn_close_now(c);
        return;
    }

    memcpy(conn->bytes + conn->bytes_len, bytes, len);
    conn->bytes_len += len;
    conn->bytes[conn->bytes_len] = '\0';

    for (;;) {
        if (conn->req == NULL) {
            conn->req = (http_request*)calloc(1, sizeof(*conn->req));
            if (conn->req == NULL) {
                tcp_conn_close_now(c);
                return;
            }
        }

        http_request* req = conn->req;
        if (!conn->parsed_headers) {
            int32_t header_bytes = parse_headers(conn, req);
            if (header_bytes == -1) return;
            if (header_bytes < 0) {
                http_response bad = response_default();
                response_set_static(&bad, "400", "Bad Request");
                (void)set_response_header(&bad, "Connection", "close");
                (void)write_response(c, req, &bad);
                log_response(c, req, &bad);
                response_cleanup(&bad);
                http_conn_reset(conn);
                return;
            }

            conn->parsed_headers = true;
            conn->bytes_off = (size_t)header_bytes;
        }

        if (!request_has_supported_version(req)) {
            http_response unsupported = response_default();
            response_set_static(&unsupported, "505", "HTTP Version Not Supported");
            (void)set_response_header(&unsupported, "Connection", "close");
            (void)write_response(c, req, &unsupported);
            log_response(c, req, &unsupported);
            response_cleanup(&unsupported);
            http_conn_reset(conn);
            return;
        }

        if (!request_expectation_supported(req)) {
            http_response failed = response_default();
            response_set_static(&failed, "417", "Expectation Failed");
            (void)set_response_header(&failed, "Connection", "close");
            (void)write_response(c, req, &failed);
            log_response(c, req, &failed);
            response_cleanup(&failed);
            http_conn_reset(conn);
            return;
        }

        size_t body_bytes = conn->bytes_len - conn->bytes_off;
        if (!conn->sent_continue && request_expects_continue(req) &&
            req->content_length > body_bytes) {
            if (!write_continue_response(c)) {
                tcp_conn_close_now(c);
                return;
            }
            conn->sent_continue = true;
        }

        if (req->content_length > body_bytes) return;

        req->body =
            req->content_length == 0 ? NULL : conn->bytes + conn->bytes_off;

        http_response res = response_default();
        char allow_header[64];
        enum Method method = get_method_from_str(req->method);
        enum Method handler_method = method;
        struct Route* r = find_route(s->router, req->route);
        route_handler handler = NULL;

        if (r != NULL && method < MAX_METHODS) {
            if (method == HEAD && r->handlers[HEAD].handler == NULL) {
                handler_method = GET;
            }

            if (handler_method < MAX_METHODS) {
                handler = r->handlers[handler_method].handler;
            }
        }

        if (s->router->mware_func != NULL) {
            s->router->mware_func(s->user_ctx, req, &res);
        }

        if (r == NULL) {
            response_set_static(&res, "404", "Not Found");
        } else if (method >= MAX_METHODS) {
            response_set_static(&res, "405", "Method Not Allowed");
        } else if (handler == NULL) {
            response_set_static(&res, "405", "Method Not Allowed");
        } else {
            handler(s->user_ctx, req, &res);
            s->total_requests++;
        }

        if (strcmp(res.status_code, "405") == 0 &&
            build_allow_header_value(r, allow_header, sizeof(allow_header))) {
            (void)set_response_header(&res, "Allow", allow_header);
        }

        bool close = response_should_close(req, &res);
        if (!write_response(c, req, &res)) {
            response_cleanup(&res);
            tcp_conn_close_now(c);
            return;
        }

        log_response(c, req, &res);

        size_t consumed = conn->bytes_off + req->content_length;
        response_cleanup(&res);
        http_conn_clear_request(conn);

        if (close) return;

        http_conn_consume_bytes(conn, consumed);
        if (conn->bytes_len == 0) return;
    }
}

ExpressServer* server_new(ExpressConfig* cnfg, ExpressRouter* router) {
    if (cnfg == NULL || router == NULL) return NULL;

    ExpressServer* server = (ExpressServer*)calloc(1, sizeof(*server));
    if (server == NULL) return NULL;

    server->user_ctx = cnfg->ctx;
    server->router = router;

    TCPServerConfig tcp_cfg;
    memset(&tcp_cfg, 0, sizeof(tcp_cfg));
    tcp_cfg.port = cnfg->port;
    tcp_cfg.ctx = server;
    tcp_cfg.on_accept = on_accept;
    tcp_cfg.on_bytes = on_bytes;
    tcp_cfg.on_close = on_close;

    server->tcp_server = tcp_server_create(&tcp_cfg);
    if (server->tcp_server == NULL) {
        free(server);
        return NULL;
    }

    return server;
}

void server_run(ExpressServer* server) {
    if (server == NULL || server->tcp_server == NULL) return;
    (void)tcp_server_run(server->tcp_server);
}

void server_destroy(ExpressServer* server) {
    if (server == NULL) return;

    tcp_server_destroy(server->tcp_server);
    free(server);
}




