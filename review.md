# ExpressC Security & Performance Review

## Security

### S1 — Memory Exhaustion DoS (Critical)

`parse_content_length_value` in `ExpressC.c` correctly prevents integer overflow but imposes no upper bound on the declared body size. A client sending `Content-Length: 2147483648` causes `ensure_http_conn_cap` to attempt a 2 GB allocation per connection while waiting for bytes that may never arrive. Multiple such connections exhaust all available RAM.

**Fix:** Add a `max_body_size` field to `ExpressConfig` (e.g. 1 MB default). In `on_bytes`, reject with 413 before calling `ensure_http_conn_cap` when `content_length` exceeds the limit.

---

### S2 — CRLF Injection in Response Headers and Cookies (Critical)

`write_response` formats response headers and `Set-Cookie` lines via `snprintf` with no sanitization:

```c
snprintf(buf, ..., "%s: %s\r\n", key, value);
snprintf(buf, ..., "Set-Cookie: %s=%s\r\n", name, value);
```

A header value or cookie value containing `\r\n` injected by an attacker can split the HTTP response, inserting arbitrary headers or splitting the response body — enabling cache poisoning or XSS depending on the deployment.

**Fix:** Before formatting each header or cookie, scan for `\r` or `\n` and either reject the response or strip those characters.

---

### S3 — Query Strings Break All Routing (Critical)

`parse_request_line` stores the full URL token — including any `?query` suffix — directly into `req->route[1024]`. `find_route` uses exact `strcmp`. A request to `/ping?foo=bar` does not match the registered `/ping` route and returns 404. Because no code ever populates `req->request_params`, the entire query-string parameter system (`get_request_param`, `request_params[]`) is dead storage.

**Fix:** In `parse_request_line`, split the URL on the first `?`, store only the path in `req->route`, and parse `key=value` pairs from the remainder into `req->request_params`.

---

### S4 — `set_response_header` Stores Raw Pointers (Critical)

`set_response_header` stores the caller's `key` and `value` pointers directly without copying:

```c
res->headers[res->headers_len].key   = (char *)key;
res->headers[res->headers_len].value = (char *)value;
```

`response_cleanup` frees `res->headers` (the array) but not the strings within it. Any caller passing a stack-allocated or heap string that is freed before the response is serialized hits a use-after-free. This is asymmetric with `set_response_cookie`, which does `malloc`/`strcpy` — making the API a trap for users.

**Fix:** `strdup` both `key` and `value` in `set_response_header`. Free them in `response_cleanup` alongside the array.

---

### S5 — Unintentional Symbol Exports (High)

The following functions are non-static but not declared in `ExpressC.h`, making them callable by any translation unit linked against `ExpressC.o`:

- `add_cookie_to_request`
- `get_cookie_copy` / `destroy_cookie_copy`
- `get_method_from_str`
- `find_route`
- `log_response`

**Fix:** Mark each as `static`, or declare them in `ExpressC.h` if they are intended public API.

---

### S6 — `on_bytes` Called Before EOF Check in TCPServer (High)

In `handle_read` (`TCPServer/TCPServer.c`), when `recv` returns 0 (peer closed connection), the `on_bytes` callback is invoked with `len = 0` and stale buffer contents before the `return false` that signals EOF:

```c
ssize_t n = recv(c->fd, buf, sizeof(buf), 0);
if (s->on_bytes) {
    s->on_bytes(s->ctx, c, buf, (size_t)n);  // called with n=0
}
if (n == 0) return false;  // checked after
```

The HTTP layer happens to handle a zero-byte `memcpy` safely, but this is a latent correctness bug.

**Fix:** Check `n <= 0` and `errno == EAGAIN` before invoking `on_bytes`. Only call `on_bytes` when `n > 0`.

---

### S7 — Unbounded Cookie Values (Minor)

Cookie parsing caps the key length at 1024 bytes but applies no length limit to cookie values. Values are bounded only by the full `Cookie:` header length.

**Fix:** Add a max per-value length check (e.g. 4096 bytes) during cookie parsing.

---

### S8 — Dead URL-Decode Code / No Path Normalization (Minor)

`url_decode_char` is fully implemented but has zero callers. Encoded sequences like `%2F%2E%2E` in paths are never decoded or normalized, meaning path traversal sequences pass through to route matching opaquely.

**Fix:** Either remove the dead function or wire it into `parse_request_line` with proper path normalization (collapse `..` segments after decoding).

---

### S9 — Undocumented Borrowed `req->body` Lifetime (Minor)

`req->body` is set to a raw pointer into `conn->bytes`:

```c
req->body = conn->bytes + conn->bytes_off;
```

After `http_conn_consume_bytes` shifts the buffer via `memmove`, any stored copy of this pointer is stale. This is not documented in `ExpressC.h`, making it an invisible constraint on handler code.

**Fix:** Document the lifetime constraint in `ExpressC.h`. Handlers must not store `req->body` beyond the duration of the handler call.

---

## Performance

### P1 — O(N) Route Lookup (High)

`find_route` iterates up to `MAX_ROUTES = 512` entries with `strcmp` on every request. At 512 routes, every dispatch pays 512 string comparisons in the worst case.

**Fix:** Replace the linear array with a hash table keyed on the route string (e.g. FNV-1a with open addressing). A 1024-slot table reduces average lookup to O(1).

---

### P2 — O(N) Header and Cookie Lookup (High)

`get_request_header`, `get_request_param`, and `get_cookie` all perform a linear scan over their full arrays on every call. With 128 headers, each lookup is up to 128 case-insensitive comparisons. Middleware or handlers that call multiple accessors pay this cost repeatedly per request.

**Fix:** Sort headers and cookies during parsing (insertion sort is fine at small N) and binary search in accessors, or use a small hash table with the same FNV approach as P1.

---

### P3 — Two `tcp_conn_write` Calls Per Response (High)

`write_response` calls `tcp_conn_write` twice per response: once for the serialized header block and once for the body. This results in two separate `send()` syscalls and two kernel buffer copies per response.

**Fix:** Use `writev(2)` with two `iovec` entries (header buffer + body pointer) to combine both writes into a single syscall, or copy small bodies directly into the header buffer when they fit.

---

### P4 — Receive Buffer Starts at 1024 Bytes (High)

`ensure_http_conn_cap` initializes the per-connection receive buffer to 1024 bytes. A typical HTTP request with headers runs 300–2000 bytes; even a modest request body triggers an immediate `realloc`.

**Fix:** Start the buffer at 8192 bytes. This covers the vast majority of HTTP requests (headers + small bodies) in a single allocation with no realloc.

---

### P5 — One `realloc` Per Header in `set_response_header` (Medium)

Every call to `set_response_header` that adds a new key calls `realloc(headers, (N+1) * sizeof(header))`. Building a response with 10 headers incurs 10 separate `realloc` calls, each potentially copying the entire array.

**Fix:** Pre-allocate a default capacity (e.g. 8 slots) in `response_default` and only realloc when that capacity is exceeded.

---

### P6 — `memmove` on Every Completed Request (Medium)

After dispatching each request, `http_conn_consume_bytes` calls `memmove` to shift any remaining bytes (pipelined requests) to the front of the buffer. This is O(remaining bytes) per request.

**Fix:** Replace the linear buffer with a ring buffer or track a read offset so consumed data can be logically discarded without moving memory until the buffer wraps or goes idle.

---

### P7 — Level-Triggered epoll (Medium)

TCPServer registers all sockets without `EPOLLET`. Level-triggered epoll re-notifies on every event loop iteration as long as data remains in the kernel buffer, even if the application already consumed all available data in `handle_read`.

**Fix:** Add `EPOLLET` to all `epoll_ctl` calls. `handle_read` already drains the socket until `EAGAIN`, so the transition to edge-triggered requires no other changes.

---

### P8 — O(N²) Header Deduplication in `set_response_header` (Medium)

Before appending a new header, `set_response_header` does a full linear scan to detect duplicates. Building a response with N distinct headers costs O(N²) comparisons in total.

**Fix:** Remove the scan-on-write if duplicate detection is not a framework contract. Callers can use `get_response_header` explicitly when they need to check for an existing value.

---

### P9 — No Chunked Transfer Encoding (Architectural)

All response bodies must be fully materialized in memory with their `Content-Length` known before any bytes are sent. Streaming responses or responses of unknown size cannot be expressed.

---

### P10 — Single Middleware Slot (Architectural)

`router_add_middleware` returns `ROUTER_ERR` if a middleware is already registered. Multiple middleware functions must be manually composed by the caller rather than chained by the framework.

---

### P11 — Non-Atomic Request Counter (Low)

`s->total_requests++` in `on_bytes` is not thread-safe. Safe today because the event loop is single-threaded, but fragile if concurrency is ever introduced.
