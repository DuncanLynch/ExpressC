#include "../ExpressTypes/ExpressHttp.h"
#include "../ExpressTypes/error.h"

#define PARSE_FAIL(req, status)                                                \
  do {                                                                         \
    free(req);                                                                 \
    return (status);                                                           \
  } while (0)

ExpressStatus parse_headers(const char *raw_request, size_t length,
                            HttpRequest *req);
ExpressStatus parse_request_line(const char *req_line, const size_t length,
                                 HttpRequest *req);
ExpressStatus parse_body(const char *req_body, size_t length, HttpRequest *req);
ExpressStatus parse_http_request(const char *raw_request, size_t length,
                                 ExpressRequest *expr_req);
ExpressStatus parse_http_response(const char *raw_response, size_t length,
                                  ExpressResponse *expr_res);
ExpressStatus parse_response_line(const char *raw_response, HttpResponse *res);
