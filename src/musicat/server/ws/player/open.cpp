#include "musicat/musicat.h"
#include "musicat/player.h"
#include "musicat/server/ws/player.h"
#include <uWebSockets/src/App.h>

namespace musicat::server::ws::player::events
{
static void
send_player_info (uws_ws_t *ws, SocketData *sdata)
{
    nlohmann::json data = util::get_playback_info_json (sdata->server_id);

    nlohmann::json d = nlohmann::json::object ({ { "e", SOCKET_EVENT_PLAYBACK_INFO }, { "d", data } });
    ws->send (d.dump ());
}

void
open (uws_ws_t *ws)
{
    const bool debug = get_debug_state ();

    if (debug)
        {
            fprintf (stderr, "[server OPEN] %lu\n", (uintptr_t)ws);
        }

    // subscribe to topics: global, player/guild_id
    auto *sdata = ws->getUserData ();
    ws->subscribe ("global");
    ws->subscribe ("player/" + sdata->server_id.str ());

    ws->send ("meow~");
    send_player_info (ws, sdata);
}

} // musicat::server::ws::player::events
