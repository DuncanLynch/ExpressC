#include "../ExpressTypes/ExpressHttp.h"

#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

ExpressStatus is_valid_method(const char *method) {
  const char *valid_methods[] = {"GET",   "POST", "PUT",     "DELETE",
                                 "PATCH", "HEAD", "OPTIONS", NULL};
  for (int i = 0; valid_methods[i] != NULL; i++) {
    if (strcmp(method, valid_methods[i]) == 0) {
      return EXPRESS_OK;
    }
  }
  return EXPRESS_PARSE_INVALID_METHOD;
}
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

  if (is_valid_method(req->method) != EXPRESS_OK) {
    // cleanup here
  }
  expr_req->url = req->url;
  expr_req->method = req->method;
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

ExpressStatus parse_request_params(const char *url, const size_t len,
                                   ExpressRequest *req) {
  const char *paramloc = strchr(url, '?');
  if (paramloc == NULL)
    return EXPRESS_PARSE_NOPARAMS;
  char *params = strdup(paramloc + 1);
  char *line = strtok(params, "&");
  Params *current_param = NULL;
  while (line) {
    char *equal = strchr(line, '=');
    if (equal) {
      Params *new_param = malloc(sizeof(Params));
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
    } else {
      free(params);
      free_params(req->param);
    }
    line = strtok(NULL, "&");
  }
  free(params);
  return EXPRESS_OK;
}

ExpressStatus print_req(ExpressRequest *req) {
  printf("Method: %s\n", req->method);
  printf("URL: %s\n", req->url);
  printf("HTTP Version: %s\n", req->httpVersion);
  printf("Headers:\n");
  ExpressHeader *header = req->headers;
  while (header) {
    printf("  %s: %s\n", header->key, header->value);
    header = header->next;
  }
  printf("Params:\n");
  Params *param = req->param;
  while (param) {
    printf("  %s: %s\n", param->key, param->value);
    param = param->next;
  }
  printf("Body: %s\n", req->body);
  return EXPRESS_OK;
}

/*
 * Request Builder Functions
 * -------------------------
 * Functions to build an Express Request, and then to convert to an HTTP Request
 * While an express request can be build manually, using these functions
 * guaruntees the proper format.
 *
 */

ExpressStatus init_request(ExpressRequest *req) {
  req = malloc(sizeof(ExpressRequest));
  if (!req)
    return EXPRESS_PARSE_MEM_ERR;
  req->method = NULL;
  req->url = NULL;
  req->httpVersion = NULL;
  req->headers = NULL;
  req->body = NULL;
  req->bodyLength = 0;
  req->timeout_ms = 0;
  req->param = NULL;
  return EXPRESS_OK;
}

ExpressStatus set_request_method(ExpressRequest *req, const char *method) {
  if (!is_valid_method(method))
    return EXPRESS_PARSE_INVALID_METHOD;
  if (req->method)
    free(req->method);
  req->method = strdup(method);
  return EXPRESS_OK;
}

ExpressStatus set_request_url(ExpressRequest *req, const char *url) {
  if (req->url)
    free(req->url);
  req->url = strdup(url);
  return EXPRESS_OK;
}

ExpressStatus set_request_http_version(ExpressRequest *req,
                                       const char *httpVersion) {
  if (req->httpVersion)
    free(req->httpVersion);
  req->httpVersion = strdup(httpVersion);
  return EXPRESS_OK;
}

ExpressStatus add_request_header(ExpressRequest *req, const char *key,
                                 const char *value) {
  if (req->headers == NULL) {
    ExpressHeader *new_header = (ExpressHeader *)malloc(sizeof(ExpressHeader));
    new_header->key = strdup(key);
    new_header->value = strdup(value);
    new_header->next = NULL;
    req->headers = new_header;
  } else {
    ExpressHeader *new_header = (ExpressHeader *)malloc(sizeof(ExpressHeader));
    new_header->key = strdup(key);
    new_header->value = strdup(value);
    new_header->next = NULL;
    ExpressHeader *curr = req->headers;
    while (curr->next) {
      curr = curr->next;
    }
    curr->next = new_header;
  }
  return EXPRESS_OK;
}

ExpressStatus set_request_body(ExpressRequest *req, const char *body,
                               size_t length) {
  if (req->body) {
    free(req->body);
  }
  req->body = strdup(body);
  req->bodyLength = length;
  return EXPRESS_OK;
}

ExpressStatus add_request_param(ExpressRequest *req, const char *key,
                                const char *value) {
  if (req->param == NULL) {
    Params *new_header = (Params *)malloc(sizeof(Params));
    new_header->key = strdup(key);
    new_header->value = strdup(value);
    new_header->next = NULL;
    req->param = new_header;
  } else {
    Params *new_header = (Params *)malloc(sizeof(Params));
    new_header->key = strdup(key);
    new_header->value = strdup(value);
    new_header->next = NULL;
    Params *curr = req->param;
    while (curr->next) {
      curr = curr->next;
    }
    curr->next = new_header;
  }
  return EXPRESS_OK;
}

ExpressStatus serialize_request(ExpressRequest *req, char *serialized) {}

/*  Memeory cleanup functions
 *  -------------------------
 *  Freeing params frees for both htttp request and express request,
 *  Freeing from an http request free might affect the express request,
 *  so only free an http request when both need to go, otherwise free from
 *  individual fields.
 */

ExpressStatus free_headers(ExpressHeader *header) {
  while (header) {
    ExpressHeader *temp = header;
    header = header->next;
    free(temp->key);
    free(temp->value);
    free(temp);
  }
  return EXPRESS_OK;
}

ExpressStatus free_params(Params *param) {
  while (param) {
    Params *temp = param;
    param = param->next;
    free(temp->key);
    free(temp->value);
    free(temp);
  }
  return EXPRESS_OK;
}

ExpressStatus free_http_request(HttpRequest *req) {
  if (req->method)
    free(req->method);
  if (req->url)
    free(req->url);
  if (req->httpVersion)
    free(req->httpVersion);
  if (req->headers)
    free_headers(req->headers);
  if (req->body)
    free(req->body);
  return EXPRESS_OK;
}

ExpressStatus free_request(ExpressRequest *req) {
  if (req->method)
    free(req->method);
  if (req->url)
    free(req->url);
  if (req->httpVersion)
    free(req->httpVersion);
  if (req->headers)
    free_headers(req->headers);
  if (req->body)
    free(req->body);
  if (req->param)
    free_params(req->param);
  free(req);
  return EXPRESS_OK;
}
ExpressStatus free_http_response(HttpResponse *res) {
  if (res->statusMessage)
    free(res->statusMessage);
  if (res->headers)
    free_headers(res->headers);
  if (res->body)
    free(res->body);
  return EXPRESS_OK;
}
ExpressStatus free_response(ExpressResponse *res) {
  if (res->method)
    free(res->method);
  if (res->url)
    free(res->url);
  if (res->statusMessage)
    free(res->statusMessage);
  if (res->headers)
    free_headers(res->headers);
  if (res->body)
    free(res->body);
  free(res);
  return EXPRESS_OK;
}
