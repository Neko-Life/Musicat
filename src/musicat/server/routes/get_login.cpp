#include "musicat/server/middlewares.h"
#include "musicat/server/response.h"
#include "musicat/server/states.h"

namespace musicat::server::routes
{

void
get_login (APIResponse *res, APIRequest *req)
{
    int status;
    status = middlewares::cors (res, req);
    if (status)
        return;

    middlewares::set_content_type_json (res);

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

    res->writeStatus (http_status)->end (r.dump ());
}

} // musicat::server::routes
