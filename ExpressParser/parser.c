#include "parser.h"
#include <stdlib.h>
#include <string.h>

const char *test_request = "GET /path/resource?id=123&foo=bar HTTP/1.1\r\n"
                           "Host: example.com\r\n"
                           "User-Agent: MyTestClient/1.0\r\n"
                           "Accept: */*\r\n"
                           "Content-Length: 13\r\n"
                           "\r\n"
                           "Hello=World!";
ExpressStatus parse_http_request(const char *raw_request, size_t length,
                                 ExpressRequest *expr_req) {
  HttpRequest *req = malloc(sizeof(HttpRequest));
  char *req_ptr = raw_request;
  char *tempreq = strstr(raw_request, "\r\n");
  ExpressStatus frontlinestatus =
      parse_request_line(raw_request, tempreq - raw_request, req);
  if (frontlinestatus != EXPRESS_OK) {
    PARSE_FAIL(req, frontlinestatus);
  }
  req_ptr += tempreq - raw_request + 2;
  tempreq = strstr(req_ptr, "\r\n\r\n");
  ExpressStatus header_status = parse_headers(req_ptr, tempreq - req_ptr, req);
  if (header_status != EXPRESS_OK) {
    PARSE_FAIL(req, header_status);
  }
  req_ptr += tempreq - req_ptr + 4;
  ExpressStatus body_status =
      parse_body(req_ptr, length - (req_ptr - raw_request), req);
  if (body_status != EXPRESS_OK) {
    PARSE_FAIL(req, body_status);
  }
  return EXPRESS_OK;
}

ExpressStatus parse_request_line(const char *req_line, const size_t length,
                                 HttpRequest *req) {}
