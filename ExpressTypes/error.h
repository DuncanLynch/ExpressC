#include <stdint.h>

typedef int8_t ExpressStatus;

/*
Errors in ExpressC are handled with status codes.
In this case, 0 is success, negative values are errors, and positive values are
warnings. Before every clump of errors/warnings, which are given a range of 10
values for each "section", you are told the range and what the clump is for. For
example, the ExpressParser errors are in the range of |10-19|.
*/
#define EXPRESS_OK 0

// ExpressParser |10-19|
// Errors
#define EXPRESS_PARSE_REQUEST_ERROR -10
#define EXPRESS_PARSE_RESPONSE_ERROR -11
#define EXPRESS_PARSE_INVALID_HTTP -12

#define EXPRESS_PARSE_NOPARAMS 10
