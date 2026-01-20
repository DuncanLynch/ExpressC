#include "../ExpressTypes/ExpressHttp.h"
#include "../ExpressTypes/error.h"

ExpressRequest* init_req();
ExpressStatus set_url(ExpressRequest* req, char* url);
ExpressStatus set_method(ExpressRequest* req, Method method);
ExpressStatus set_body(ExpressRequest* req, char* body);
ExpressStatus set_timeout(ExpressRequest* req, unsigned int ms);

ExpressResponse send_req(ExpressRequest* req);

ExpressPromise* send_req_async(ExpressRequest* req);
ExpressResponse* req_await(ExpressPromise* promise);