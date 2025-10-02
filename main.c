#include "ExpressParser/parser.h"
#include "ExpressTypes/ExpressHttp.h"
#include <stdlib.h>
#include <string.h>

char *test_request = "GET /path/resource?id=123&foo=bar HTTP/1.1\r\n"
                     "Host: example.com\r\n"
                     "User-Agent: MyTestClient/1.0\r\n"
                     "Accept: */*\r\n"
                     "Content-Length: 13\r\n"
                     "\r\n"
                     "Hello=World!";

int main() {
  ExpressRequest *req = malloc(sizeof(ExpressRequest));
  parse_http_request(test_request, strlen(test_request), req);
}
