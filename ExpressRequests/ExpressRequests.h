#pragma once

#include "../ExpressTypes/ExpressHttp.h"
#include "../ExpressTypes/error.h"

ExpressStatus send_req(ExpressRequest *req, ExpressResponse *res);

ExpressPromise *send_req_async(ExpressRequest *req);
ExpressResponse *req_await(ExpressPromise *promise);
