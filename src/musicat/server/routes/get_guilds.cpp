#include "musicat/server/middlewares.h"
#include "musicat/server/response.h"

namespace musicat::server::routes
{

void
get_guilds (APIResponse *res, APIRequest *req)
{
    auto cors_headers = middlewares::cors (res, req);
    if (cors_headers.empty ())
        return;

    /* middlewares::print_headers (req); */

    std::string user_id = middlewares::validate_token (res, req, cors_headers);
    if (user_id.empty ())
        return;

    const char *http_status = http_status_t.OK_200;
    nlohmann::json r;

    // !TODO: request user guilds and save to cache

    res->writeStatus (http_status);
    middlewares::write_headers (res, cors_headers);
    middlewares::set_content_type_json (res);
    res->end (r.dump ());
}

} // musicat::server::routes
