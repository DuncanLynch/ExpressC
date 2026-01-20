#ifndef PARSER_H
#define PARSER_H
#include "../ExpressTypes/ExpressHttp.h"
#include "../ExpressTypes/error.h"

#define PARSE_FAIL(req, status)                                                \
  do {                                                                         \
    free(req);                                                                 \
    return (status);                                                           \
  } while (0)

ExpressStatus is_valid_method(const char *method);

ExpressStatus parse_headers(const char *raw_request, size_t length,
                            HttpRequest *req);
ExpressStatus parse_request_line(const char *req_line, const size_t length,
                                 HttpRequest *req);
ExpressStatus parse_body(const char *req_body, size_t length, HttpRequest *req);
ExpressStatus parse_http_request(char *raw_request, size_t length,
                                 ExpressRequest *expr_req);
ExpressStatus parse_http_response(const char *raw_response, size_t length,
                                  ExpressResponse *expr_res);
ExpressStatus parse_response_line(const char *raw_response, HttpResponse *res);
ExpressStatus parse_request_params(const char *url, const size_t len,
                                   ExpressRequest *req);

ExpressStatus print_req(ExpressRequest *req);
ExpressStatus print_res(ExpressResponse *res);

// Request builder functions
ExpressStatus init_request(ExpressRequest *req);
ExpressStatus set_request_method(ExpressRequest *req, const char *method);
ExpressStatus set_request_url(ExpressRequest *req, const char *url);
ExpressStatus set_request_http_version(ExpressRequest *req,
                                       const char *httpVersion);
ExpressStatus add_request_header(ExpressRequest *req, const char *key,
                                 const char *value);
ExpressStatus set_request_body(ExpressRequest *req, const char *body,
                               size_t length);
ExpressStatus set_request_timeout(ExpressRequest* req, unsigned int ms);
ExpressStatus add_request_param(ExpressRequest *req, const char *key,
                                const char *value);
ExpressStatus serialize_request(ExpressRequest *req, char *serialized);
// Memory cleanup functions
ExpressStatus free_request(ExpressRequest *req);
ExpressStatus free_response(ExpressResponse *res);
ExpressStatus free_headers(ExpressHeader *header);
ExpressStatus free_params(Params *param);
ExpressStatus free_http_request(HttpRequest *req);
ExpressStatus free_http_response(HttpResponse *res);
#endif
