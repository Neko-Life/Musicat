#include "musicat/server/middlewares.h"
#include "musicat/server/response.h"
#include "musicat/server/states.h"

namespace musicat::server::routes
{
#ifndef MUSICAT_NO_SERVER

void
get_login (APIResponse *res, APIRequest *req)
{
    auto cors_headers = middlewares::cors (res, req);
    if (cors_headers.empty ())
        return;

    /* middlewares::print_headers(req); */

    const char *http_status = http_status_t.OK_200;
    nlohmann::json r;

    std::string oauth_state = get_oauth_link ();
    if (!oauth_state.length ())
        {
            http_status = http_status_t.INTERNAL_SERVER_ERROR_500;
            r = response::error (response::ERROR_CODE_NOTHING,
                                 "No OAuth configured");
        }
    else
        {
            std::string redir (req->getHeader ("redirect-param"));
            std::string state = states::generate_oauth_state (redir);

            r = response::payload (oauth_state + "&state=" + state);
        }

    res->writeStatus (http_status);
    middlewares::write_headers (res, cors_headers);
    middlewares::set_content_type_json (res);
    res->end (r.dump ());
}

#endif
} // musicat::server::routes
