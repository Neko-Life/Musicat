#include "musicat/db.h"
#include "musicat/server/middlewares.h"
#include "musicat/server/response.h"
#include "musicat/server/service_cache.h"

namespace musicat::server::routes
{

void
post_logout (APIResponse *res, APIRequest *req)
{
    auto cors_headers = middlewares::cors (res, req);
    if (cors_headers.empty ())
        return;

    std::string user_id = middlewares::validate_token (res, req, cors_headers);
    if (user_id.empty ())
        return;

    response::end_t endres (res);
    endres.headers = cors_headers;

    // remove user auth in cache and db
    database::delete_user_auth (user_id);
    service_cache::remove_cached_user_auth (user_id);

    // reset token cookie
    endres.add_header ("Set-Cookie", std::string ("token=; Max-Age=0; HttpOnly"));
}

} // musicat::server::routes
