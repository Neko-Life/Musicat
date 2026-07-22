#include "musicat/musicat.h"
#include "musicat/player.h"
#include "musicat/server/ws/player.h"
#include <uWebSockets/src/App.h>

namespace musicat::server::ws::player::events
{

static void
send_player_fx_info (uws_ws_t *ws, SocketData *sdata)
{
    auto *pm = get_player_manager_ptr ();
    if (!pm)
        return;
    auto gp = pm->get_player (sdata->server_id);
    if (!gp)
        return;

    nlohmann::json d = nlohmann::json::object ({ { "e", SOCKET_EVENT_FX }, { "d", gp->fx_states_to_json () } });
    ws->send (d.dump ());
}

static void
send_player_queue_info (uws_ws_t *ws, SocketData *sdata)
{
    auto *player_manager = get_player_manager_ptr ();
    if (!player_manager)
        return;

    auto q = player_manager->get_queue (sdata->server_id);

    auto a = nlohmann::json::array ();
    for (auto &t : q)
        {
            auto d = nlohmann::json::object ();
            util::set_playback_info_track_data (d, sdata->server_id, t);
            a.push_back (d);
        }

    nlohmann::json d = nlohmann::json::object ({ { "e", SOCKET_EVENT_QUEUE }, { "d", a } });
    ws->send (d.dump ());
}

static void
send_player_info (uws_ws_t *ws, SocketData *sdata)
{
    // playback_info
    nlohmann::json data = util::get_playback_info_json (sdata->server_id);

    nlohmann::json d = nlohmann::json::object ({ { "e", SOCKET_EVENT_PLAYBACK_INFO }, { "d", data } });
    ws->send (d.dump ());

    // fx state if playing
    if (auto i = data.find ("title") != data.end ())
        {
            send_player_fx_info (ws, sdata);
        }

    // queue
    send_player_queue_info (ws, sdata);
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
    ws->subscribe (sdata->get_player_topic ());

    ws->send ("meow~");
    send_player_info (ws, sdata);
}

} // musicat::server::ws::player::events
