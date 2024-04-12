#include "musicat/server/middlewares.h"
#include "musicat/server/response.h"

namespace musicat::server::routes
{
#ifndef MUSICAT_NO_SERVER

void
get_root (APIResponse *res, APIRequest *req)
{
    auto cors_headers = middlewares::cors (res, req);
    if (cors_headers.empty ())
        return;

    /* middlewares::print_headers (req); */

    const char *http_status = http_status_t.OK_200;
    nlohmann::json r;

    auto bot = get_client_ptr ();
    if (!bot)
        {
            http_status = http_status_t.INTERNAL_SERVER_ERROR_500;
            r = response::error (response::ERROR_CODE_NOTHING,
                                 "Bot not running");
        }
    else
        r = response::payload (
            { { "avatar_url",
                bot->me.get_avatar_url (BOT_AVATAR_SIZE, dpp::i_webp) },
              { "username", bot->me.username },
              { "description", get_bot_description () } });

    res->writeStatus (http_status);
    middlewares::write_headers (res, cors_headers);
    middlewares::set_content_type_json (res);
    res->end (r.dump ());
}

#endif
} // musicat::server::routes
