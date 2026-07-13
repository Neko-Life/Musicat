#include "musicat/musicat.h"
#include "musicat/server/ws/player.h"
#include <uWebSockets/src/App.h>

namespace musicat::server::ws::player::events
{

nlohmann::json
get_bot_info_payload (dpp::cluster *bot)
{
    bool debug = get_debug_state ();

    nlohmann::json d
        = { { "avatarUrl",
              bot->me.get_avatar_url (BOT_AVATAR_SIZE, dpp::i_webp) },
            { "username", bot->me.username },
            { "description", get_bot_description () } };

    if (debug)
        fprintf (stderr, "[server get_bot_info_payload]:\n%s\n",
                 d.dump (2).c_str ());

    return d;
}

void
open (uws_ws_t *ws)
{
    const bool debug = get_debug_state ();

    if (debug)
        {
            fprintf (stderr, "[server OPEN] %lu\n", (uintptr_t)ws);
        }

    ws->subscribe ("bot_info_update");

    auto bot = get_client_ptr ();

    if (!bot)
        {
            return;
        }

    nlohmann::json payload_bot_info = get_bot_info_payload (bot);
    ws->send (payload_bot_info.dump ());

    // send any random active player info
}

} // musicat::server::ws::player::events
