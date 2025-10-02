#ifndef EXPRESS_HTTP_H
#define EXPRESS_HTTP_H

#include <stddef.h>
#define MAX_BUFFER_SIZE 8192

typedef struct ExpressHeader ExpressHeader;

struct ExpressHeader {
  char *key;
  char *value;
  ExpressHeader *next;
};

typedef struct Params {
  char *key;
  char *value;
  struct Params *next;
} Params;

typedef struct HttpRequest {
  char *method;
  char *url;
  char *httpVersion;
  ExpressHeader *headers;
  Params *params;
  char *body;
} HttpRequest;

typedef struct HttpResponse {
  int statusCode;
  char *statusMessage;
  ExpressHeader *headers;
  char *body;
  size_t bodyLength;
} HttpResponse;

typedef enum Method { GET, POST, PUT, DELETE, PATCH, OPTIONS, HEAD } Method;

typedef struct ExpressResponse {
  Method method;
  int statusCode;
  char *statusMessage;
  char *url;
  ExpressHeader *headers;
  size_t headerCount;
  char *body;
  size_t bodyLength;
  unsigned int timeout_ms;
} ExpressResponse;

typedef struct ExpressRequest {
  char *url;
  Method method;
  ExpressHeader *headers;
  size_t headerCount;
  char *body;
  size_t bodyLength;
  unsigned int timeout_ms;
} ExpressRequest;

#endif
