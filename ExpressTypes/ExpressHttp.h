#include <stddef.h>
#ifndef EXPRESS_HTTP_H
#define EXPRESS_HTTP_H

#define MAX_BUFFER_SIZE 8192

typedef struct Header {
  char *key;
  char *value;
} Header;

typedef struct HttpRequest {
  char *method;
  char *url;
  char *httpVersion;
  Header *headers;
  char *body;
} HttpRequest;

typedef struct HttpResponse {
  int statusCode;
  char *statusMessage;
  Header *headers;
  size_t headerlCount;
  char *body;
  size_t bodyLength;
} HttpResponse;

typedef enum Method { GET, POST, PUT, DELETE, PATCH, OPTIONS, HEAD } Method;

typedef struct ExpressResponse {
  Method method;
  int statusCode;
  char *statusMessage;
  char *url;
  Header *headers;
  size_t headerCount;
  char *body;
  size_t bodyLength;
  unsigned int timeout_ms;
} ExpressResponse;

typedef struct ExpressRequest {
  char *url;
  Method method;
  Header *headers;
  size_t headerCount;
  char *body;
  size_t bodyLength;
  unsigned int timeout_ms;
} ExpressRequest;

#endif
