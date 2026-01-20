#pragma once

#include "../ExpressTypes/ExpressHttp.h"
#include "../ExpressTypes/error.h"

ExpressResponse send_req(ExpressRequest* req);

ExpressPromise* send_req_async(ExpressRequest* req);
ExpressResponse* req_await(ExpressPromise* promise);