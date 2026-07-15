#include "musicat/server/middlewares.h"
#include "musicat/server/response.h"

namespace musicat::server::routes
{

void
post_logout (APIResponse *res, APIRequest *req)
{
    auto cors_headers = middlewares::cors (res, req);
    if (cors_headers.empty ())
        return;

    /* middlewares::print_headers (req); */

    std::string user_id = middlewares::validate_token (res, req, cors_headers);
    if (user_id.empty ())
        return;

    response::end_t endres (res);
    endres.headers = cors_headers;

    // do stuff
}

} // musicat::server::routes
