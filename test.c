// main.c
#include "ExpressParser/parser.h"
#include "ExpressRequests/ExpressRequests.h"
#include "ExpressTypes/ExpressHttp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_response(const ExpressResponse *res) {
  printf("=== Response ===\n");
  printf("Method: %s\n", res->method ? res->method : "(null)");
  printf("URL: %s\n", res->url ? res->url : "(null)");
  printf("Status: %d %s\n", res->statusCode,
         res->statusMessage ? res->statusMessage : "");

  printf("Headers:\n");
  for (ExpressHeader *h = res->headers; h; h = h->next) {
    printf("  %s: %s\n", h->key ? h->key : "(null)",
           h->value ? h->value : "(null)");
  }

  printf("BodyLength: %zu\n", res->bodyLength);
  if (res->body) {
    size_t to_print = res->bodyLength;
    if (to_print > 800)
      to_print = 800;

    printf("Body (first %zu bytes):\n", to_print);
    fwrite(res->body, 1, to_print, stdout);
    if (to_print < res->bodyLength)
      printf("\n... (truncated)\n");
    else
      printf("\n");
  } else {
    printf("Body: (null)\n");
  }

  printf("===============\n");
}

int main(void) {
  ExpressRequest *req = NULL;
  ExpressResponse *res = NULL;

  // 1) Build request using your builder API (lives in parser.c per your code)
  ExpressStatus st = init_request(&req);
  if (st != EXPRESS_OK || !req) {
    fprintf(stderr, "init_request failed: %d\n", st);
    return 1;
  }

  // NOTE: Your set_request_method() currently has a bug:
  //   if (!is_valid_method(method)) ...
  // because is_valid_method returns ExpressStatus (EXPRESS_OK likely 0).
  // If you haven't fixed that yet, this call may fail even for "GET".
  st = set_request_method(req, "GET");
  if (st != EXPRESS_OK) {
    fprintf(stderr, "set_request_method failed: %d (fix validation logic)\n",
            st);
    goto cleanup;
  }

  st = set_request_url(req, "http://example.com/");
  if (st != EXPRESS_OK) {
    fprintf(stderr, "set_request_url failed: %d\n", st);
    goto cleanup;
  }

  st = set_request_http_version(req, "HTTP/1.1");
  if (st != EXPRESS_OK) {
    fprintf(stderr, "set_request_http_version failed: %d\n", st);
    goto cleanup;
  }

  st = set_request_timeout(req, 5000);
  if (st != EXPRESS_OK) {
    fprintf(stderr, "set_request_timeout failed: %d\n", st);
    goto cleanup;
  }

  // Helpful headers
  st = add_request_header(req, "User-Agent", "ExpressC/0.1");
  if (st != EXPRESS_OK) {
    fprintf(stderr, "add_request_header(User-Agent) failed: %d\n", st);
    goto cleanup;
  }

  st = add_request_header(req, "Accept", "*/*");
  if (st != EXPRESS_OK) {
    fprintf(stderr, "add_request_header(Accept) failed: %d\n", st);
    goto cleanup;
  }

  // Makes receive logic deterministic if server would otherwise keep-alive.
  st = add_request_header(req, "Connection", "close");
  if (st != EXPRESS_OK) {
    fprintf(stderr, "add_request_header(Connection) failed: %d\n", st);
    goto cleanup;
  }

  // Optional: ensure Host is present (your send_req adds it if missing, but no
  // harm).
  st = add_request_header(req, "Host", "example.com");
  if (st != EXPRESS_OK) {
    fprintf(stderr, "add_request_header(Host) failed: %d\n", st);
    goto cleanup;
  }

  // 2) Allocate response (your free_response() frees res itself, so
  // heap-allocate)
  res = (ExpressResponse *)calloc(1, sizeof(*res));
  if (!res) {
    fprintf(stderr, "calloc(res) failed\n");
    goto cleanup;
  }

  // 3) Send request + parse response
  st = send_req(req, res);
  if (st != EXPRESS_OK) {
    fprintf(stderr, "send_req failed: %d\n", st);
    // still print what we got if any
  }

  print_response(res);

cleanup:
  if (req)
    free_request(req);
  if (res)
    free_response(res);
  return (st == EXPRESS_OK) ? 0 : 1;
}
