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
  expr_req->url = req->url;
  switch (req->method[0]) {
  case 'G':
    expr_req->method = GET;
    break;
  case 'P':
    if (req->method[1] == 'O')
      expr_req->method = POST;
    else if (req->method[1] == 'U')
      expr_req->method = PUT;
    else if (req->method[1] == 'A')
      expr_req->method = PATCH; 
    else
      PARSE_FAIL(req, EXPRESS_PARSE_REQUEST_ERROR); 
    break;
  case 'D':
    expr_req->method = DELETE;
    break;
  case 'O':
    expr_req->method = OPTIONS;
    break;
  case 'H':
    expr_req->method = HEAD;
    break;
  default:  
    PARSE_FAIL(req, EXPRESS_PARSE_REQUEST_ERROR);
  }
  expr_req->httpVersion = req->httpVersion;
  expr_req->headers = req->headers;
  expr_req->body = req->body;
  expr_req->bodyLength = strlen(req->body);
  expr_req->timeout_ms = 0;
  expr_req->param = NULL;
  parse_request_params(expr_req->url, strlen(expr_req->url), expr_req);
  free(req);
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

ExpressStatus parse_request_params(const char* url, const size_t len, ExpressRequest* req){
  const char* paramloc = strchr(url, '?');
  if (paramloc == NULL) return EXPRESS_PARSE_NOPARAMS;
  char* params = strdup(paramloc + 1);
  char* line = strtok(params, "&");
  Params* current_param = NULL;
  while (line) {
    char* equal = strchr(line, '=');
    if (equal) {
      Params* new_param = malloc(sizeof(Params));
      *equal = '\0';
      new_param->key = strdup(line);
      new_param->value = strdup(equal + 1);
      new_param->next = NULL;
      if (!req->param) {
        req->param = new_param;
        current_param = req->param;
      } else {
        current_param->next = new_param;
        current_param = new_param;
      }
    }
    line = strtok(NULL, "&");
  }
  free(params);
  return EXPRESS_OK;
} 

ExpressStatus print_req(ExpressRequest* req){
  printf("Method: %d\n", req->method);
  printf("URL: %s\n", req->url);
  printf("HTTP Version: %s\n", req->httpVersion);
  printf("Headers:\n");
  ExpressHeader* header = req->headers;
  while (header) {
    printf("  %s: %s\n", header->key, header->value);
    header = header->next;
  }
  printf("Params:\n");
  Params* param = req->param;
  while (param) {
    printf("  %s: %s\n", param->key, param->value);
    param = param->next;
  }
  printf("Body: %s\n", req->body);
  return EXPRESS_OK;
}