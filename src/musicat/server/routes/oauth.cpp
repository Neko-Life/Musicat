#include "musicat/server/routes/oauth.h"
#include "musicat/server/middlewares.h"
#include "musicat/server/response.h"
#include "musicat/server/states.h"

namespace musicat::server::routes::oauth
{

void
get_oauth_handler (APIResponse *res, APIRequest *req,
                   std::string (*get_link) (void))
{
    auto cors_headers = middlewares::cors (res, req);
    if (cors_headers.empty ())
        return;

    /* middlewares::print_headers(req); */

    const char *http_status = http_status_t.OK_200;
    nlohmann::json r;

    const std::string link = get_link ();
    if (link.empty ())
        {
            http_status = http_status_t.INTERNAL_SERVER_ERROR_500;
            r = response::error (response::ERROR_CODE_NOTHING,
                                 "No OAuth configured");
        }
    else
        {
            const std::string redir (req->getHeader ("redirect-param"));
            const std::string state = states::generate_oauth_state (redir);

            r = response::payload (link + "&state=" + state);
        }

    res->writeStatus (http_status);
    middlewares::write_headers (res, cors_headers);
    middlewares::set_content_type_json (res);
    res->end (r.dump ());
}

} // musicat
