#include "ExpressParser/parser.h"
#include "ExpressTypes/ExpressHttp.h"
#include <stdlib.h>
#include <string.h>

char *test_request =
    "PATCH /path/to/Shreyas/heart?id=dlynch3&love=true HTTP/1.1\r\n"
    "Host: duncanlynch.com\r\n"
    "User-Agent: DlynchLoveAlgo/2.0\r\n"
    "Accept: */*\r\n"
    "Content-Length: 13\r\n"
    "\r\n"
    "{love_target: dlynch3,love_name: \"Duncan Lynch\"}";

int main() {
  ExpressRequest *req = malloc(sizeof(ExpressRequest));
  parse_http_request(test_request, strlen(test_request), req);
  print_req(req);
}
