#include "musicat/player.h"
#include "musicat/musicat.h"
#include "musicat/server.h"
#include "musicat/server/ws/player_events.h"
#include <uWebSockets/src/App.h>

namespace musicat::server::ws::player
{

// SocketData impl

SocketData::SocketData () : waved (false) {}
SocketData::SocketData (const dpp::snowflake &_server_id) : server_id (_server_id), waved (false) {}

std::string
SocketData::get_player_topic () const
{
    return ::musicat::server::ws::player::get_player_topic (server_id);
}

static std::map<uws_ws_t *, dpp::snowflake> umap;

int
register_ws_user (uws_ws_t *ws, const dpp::snowflake &user_id)
{
    umap[ws] = user_id;
    ws->getUserData ()->user_id = user_id;
    return 0;
}

int
unregister_ws_user (uws_ws_t *ws)
{
    auto i = umap.find (ws);
    if (i == umap.end ())
        return -1;

    umap.erase (i);
    return 0;
}

////////////////////////////////////////////////////////////////////////////////

std::string
get_player_topic (const dpp::snowflake &guild_id)
{
    return "player/" + guild_id.str ();
}

APIApp::WebSocketBehavior<SocketData>
get_behavior ()
{
    // use default uws options outside handlers
    APIApp::WebSocketBehavior<SocketData> b;

    b.upgrade = events::upgrade;
    b.open = events::open;
    b.message = events::message;
    b.drain = events::drain;
    b.ping = events::ping;
    b.pong = events::pong;
    b.subscription = events::subscription;
    b.close = events::close;

    return b;
}

////////////////////////////////////////////////////////////////////////////////

void
publish_event (const dpp::snowflake &guild_id, const socket_event_e event, const nlohmann::json &data)
{
    const nlohmann::json d = nlohmann::json::object ({ { "e", event }, { "d", data } });
    server::publish (get_player_topic (guild_id), d.dump ());
}

void
publish_error (const dpp::snowflake &guild_id, const nlohmann::json &err)
{
    publish_event (guild_id, SOCKET_EVENT_ERROR, { { "error", true }, { "error_message", err } });
}

void
publish_pause (const dpp::snowflake &guild_id)
{
    publish_event (guild_id, SOCKET_EVENT_PAUSE, nullptr);
}

void
publish_playback_info (const dpp::snowflake &guild_id)
{
    nlohmann::json data = util::get_playback_info_json (guild_id);
    publish_event (guild_id, SOCKET_EVENT_PLAYBACK_INFO, data);
}

void
publish_play (const dpp::snowflake &guild_id)
{
    publish_event (guild_id, SOCKET_EVENT_PLAY, nullptr);
}

void
publish_seek (const dpp::snowflake &guild_id, const uint64_t seek_ms)
{
    publish_event (guild_id, SOCKET_EVENT_SEEK, seek_ms);
}

void
publish_stop (const dpp::snowflake &guild_id)
{
    publish_event (guild_id, SOCKET_EVENT_STOP, nullptr);
}

void
publish_fx (const dpp::snowflake &guild_id)
{
    auto *player_manager = get_player_manager_ptr ();
    if (!player_manager)
        return;

    auto guild_player = player_manager->get_player (guild_id);
    if (!guild_player)
        return;

    publish_event (guild_id, SOCKET_EVENT_FX, guild_player->fx_states_to_json ());
}

void
publish_queue (const dpp::snowflake &guild_id)
{
    auto *player_manager = get_player_manager_ptr ();
    if (!player_manager)
        return;

    auto q = player_manager->get_queue (guild_id);

    auto a = nlohmann::json::array ();
    for (auto &t : q)
        {
            auto d = nlohmann::json::object ();
            util::set_playback_info_track_data (d, guild_id, t);
            a.push_back (d);
        }
    publish_event (guild_id, SOCKET_EVENT_QUEUE, a);
}

/*
   event payload:
   {
        "e": enum,
        "d": data
   }

   error data:
   {
        "code": enum,
        "message": string,
        "status": http code
   }
*

nlohmann::json
create_error_data (const socket_err_code_e code, const std::string &message,
                   const http_status_code_e status)
{
    nlohmann::json d;

    return d;
}

// utils to create json payload
nlohmann::json
create (const socket_event_e event, const nlohmann::json &data)
{
}
*/

} // musicat::server::ws::player
