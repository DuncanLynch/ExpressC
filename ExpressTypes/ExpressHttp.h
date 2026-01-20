#ifndef EXPRESS_HTTP_H
#define EXPRESS_HTTP_H

#include <stddef.h>
#include <sys/types.h>

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
  char *method;
  int statusCode;
  char *statusMessage;
  char *url;
  ExpressHeader *headers;
  char *body;
  size_t bodyLength;
  unsigned int timeout_ms;
} ExpressResponse;

typedef struct ExpressPromise {
  pid_t pid;
  ExpressResponse* res;
} ExpressPromise;

typedef struct ExpressRequest {
  char *url;
  char *method;
  char *httpVersion;
  ExpressHeader *headers;
  Params *param;
  char *body;
  size_t bodyLength;
  unsigned int timeout_ms;
} ExpressRequest;

#endif
