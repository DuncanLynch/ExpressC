#include "../ExpressTypes/ExpressHttp.h"

#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

ExpressStatus parse_http_request(char *raw_request, size_t length,
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

  printf("Method: %s\nUrl: %s\nVersion: %s\n", req->method, req->url,
         req->httpVersion);
  ExpressHeader *current_header = req->headers;
  while (current_header && current_header->key) {
    printf("Header:\nKey: %s, Value: %s\n", current_header->key,
           current_header->value);
    current_header = current_header->next;
  }

  return EXPRESS_OK;
}

ExpressStatus parse_request_line(const char *req_line, const size_t length,
                                 HttpRequest *req) {
  char *new_line = strndup(req_line, length);
  char *line = strtok(new_line, " ");
  if (strcmp(new_line, line) != 0)
    return EXPRESS_PARSE_REQUEST_ERROR;
  req->method = strdup(line);
  line = strtok(NULL, " ");
  if (line == NULL)
    return EXPRESS_PARSE_REQUEST_ERROR;
  req->url = strdup(line);
  line = strtok(NULL, " ");
  if (line == NULL)
    return EXPRESS_PARSE_REQUEST_ERROR;
  req->httpVersion = strdup(line);
  free(new_line);
  return EXPRESS_OK;
}

ExpressStatus parse_headers(const char *headers, const size_t length,
                            HttpRequest *req) {
  char *header_str = strndup(headers, length);
  char *line = strtok(header_str, "\r\n");
  ExpressHeader *first_header = NULL;
  ExpressHeader *current_header = NULL;

  while (line) {
    char *colon = strchr(line, ':');
    if (colon) {
      ExpressHeader *new_header = malloc(sizeof(ExpressHeader));
      *colon = '\0';
      new_header->key = strdup(line);
      new_header->value = strdup(colon + 2);
      new_header->next = NULL;

      if (!first_header) {
        first_header = new_header;
        current_header = first_header;
      } else {
        current_header->next = new_header;
        current_header = new_header;
      }
    }
    line = strtok(NULL, "\r\n");
  }

  req->headers = first_header;
  free(header_str);
  return EXPRESS_OK;
}

ExpressStatus parse_body(const char *body, size_t length, HttpRequest *req) {
  req->body = strndup(body, length);
  return EXPRESS_OK;
}
