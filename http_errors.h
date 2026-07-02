#pragma once

// parse_headers() error codes
#define PARSE_HEADERS_ERR_INCOMPLETE      (-1)
#define PARSE_HEADERS_ERR_REQUEST_LINE    (-2)
#define PARSE_HEADERS_ERR_INVALID         (-3)
#define PARSE_HEADERS_ERR_CAPACITY        (-4)
#define PARSE_HEADERS_ERR_CONTENT_LENGTH  (-5)
#define PARSE_HEADERS_ERR_ALLOC           (-6)

// add_cookie_to_request() error codes
#define ADD_COOKIE_ERR_CAPACITY  (-1)
#define ADD_COOKIE_ERR_NULL_ARG  (-2)

// router_add() / router_add_middleware() error codes
#define ROUTER_ERR  (-1)
