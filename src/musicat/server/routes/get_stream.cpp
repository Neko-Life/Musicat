#include "musicat/server/routes/get_stream.h"
#include "musicat/musicat.h"
#include "musicat/server.h"
#include "musicat/server/middlewares.h"
#include "musicat/server/response.h"
#include "musicat/server/stream.h"
#include <dpp/dpp.h>

namespace musicat::server::routes
{

/// actual endpoint handler
void
get_stream (APIResponse *res, APIRequest *req)
{
    auto res_headers = middlewares::cors (res, req);
    if (res_headers.empty ())
        return;

    dpp::snowflake guild_id (req->getParameter (0));

    dpp::guild *guild = guild_id.empty () ? nullptr : dpp::find_guild (guild_id);

    auto *player_manager = get_player_manager_ptr ();
    auto guild_player = player_manager ? player_manager->get_player (guild_id) : nullptr;

    // no guild or no player
    if (guild == nullptr || !guild_player)
        {
            response::end_t endres (res);
            endres.status = http_status_t.NOT_FOUND_404;
            endres.headers = res_headers;

            return;
        }

    ///
    res_headers.push_back (std::make_pair ("Content-Type", "audio/ogg; codecs=opus"));
    res_headers.push_back (std::make_pair ("Cache-Control", "max-age=0, no-cache, no-store"));
    res_headers.push_back (std::make_pair ("Pragma", "no-cache"));
    res_headers.push_back (std::make_pair ("Connection", "close"));

    res->writeStatus (http_status_t.OK_200);
    middlewares::write_headers (res, res_headers);

    // subscribe to the guild stream state
    stream::subscribe (res, guild_id);

    res->onAborted ([res] () { stream::unsubscribe (res, true); });
}

} // musicat::server::routes
