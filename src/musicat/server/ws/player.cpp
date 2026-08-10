// clang-format off
#include "musicat/player.h"
#include "musicat/player_manager.h"
// clang-format on

#include "musicat/musicat.h"
#include "musicat/player_manager_util.h"
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

using map_ws_user = std::map<uws_ws_t *, dpp::snowflake>;
static exclusive_container<map_ws_user> umap;

int
register_ws_user (uws_ws_t *ws, const dpp::snowflake &user_id)
{
    {
        auto lk = umap.acquire ();
        umap.get ()[ws] = user_id;
    }
    ws->getUserData ()->user_id = user_id;
    return 0;
}

uws_ws_t *
get_user_ws (const dpp::snowflake &user_id)
{
    auto lk = umap.acquire ();
    auto i = umap.get ().begin ();
    while (i != umap.get ().end ())
        {
            if (i->second != user_id)
                {
                    i++;
                    continue;
                }

            return i->first;
        }

    return nullptr;
}

int
unregister_ws_user (uws_ws_t *ws)
{
    auto lk = umap.acquire ();
    auto i = umap.get ().find (ws);
    if (i == umap.get ().end ())
        return -1;

    umap.get ().erase (i);
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

nlohmann::json
get_queue_payload (const dpp::snowflake &guild_id)
{
    auto q = ::musicat::player::manager::get_queue (guild_id);

    auto payload = nlohmann::json::array ();
    for (auto &t : q)
        {
            auto d = nlohmann::json::object ();
            util::set_playback_info_track_data (d, guild_id, t);
            payload.push_back (d);
        }

    return payload;
}

////////////////////////////////////////////////////////////////////////////////

/*
   event payload:
   {
        "e": enum,
        "d": data
   }

   error data:
   {
        "e": SOCKET_EVENT_ERROR,
        "d": {
            "error": true,
            "error_message": string,
        }
   }
*/

static std::string
binary_event (const socket_binary_event_e event, const std::string &data)
{
    const char ev[2] = { event, '\0' };
    return std::string (ev) + data;
}

void
publish_event (const dpp::snowflake &guild_id, const socket_event_e event, const nlohmann::json &data)
{
    const nlohmann::json d = nlohmann::json::object ({ { "e", event }, { "d", data } });
    server::publish (get_player_topic (guild_id), d.dump ());
}

void
publish_event_binary (const dpp::snowflake &guild_id, const socket_binary_event_e event, const std::string &data)
{
    server::publish (get_player_topic (guild_id), binary_event (event, data));
}

static void
publish_int32_event (const dpp::snowflake &guild_id, const socket_binary_event_e event_binary, const uint32_t n)
{
    if (n < 10000)
        {
            publish_event_binary (guild_id, event_binary, std::to_string (n));
            return;
        }

    const std::vector<char> s = std::vector<char> ({ (char)((n & 0xff000000) >> (3 * 8)), char ((n & 0x00ff0000) >> (2 * 8)),
                                                     (char)((n & 0x0000ff00) >> (1 * 8)), (char)((n & 0x000000ff)) });

    // use binary type to reduce bandwidth as this is called around 16 times per second
    publish_event_binary (guild_id, event_binary, { s.begin (), s.end () });
}

static void
send_event (const dpp::snowflake &user_id, const std::string &data)
{
    server::defer (
        [user_id, data] ()
            {
                auto *ws = get_user_ws (user_id);
                if (!ws)
                    return;

                ws->send (data);
            });
}

void
send_event (const dpp::snowflake &user_id, const socket_event_e event, const nlohmann::json &data)
{
    const nlohmann::json d = nlohmann::json::object ({ { "e", event }, { "d", data } });
    send_event (user_id, d.dump ());
}

void
send_event_binary (const dpp::snowflake &user_id, const socket_binary_event_e event, const std::string &data)
{
    send_event (user_id, binary_event (event, data));
}

////////////////////////////////////////////////////////////////////////////////

void
publish_error (const dpp::snowflake &guild_id, const nlohmann::json &err)
{
    publish_event (guild_id, SOCKET_EVENT_ERROR, { { "error", true }, { "error_message", err } });
}

void
publish_pause (const dpp::snowflake &guild_id)
{
    publish_event_binary (guild_id, SOCKET_BINARY_EVENT_PAUSE);
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
    publish_event_binary (guild_id, SOCKET_BINARY_EVENT_PLAY);
}

void
publish_seek (const dpp::snowflake &guild_id, const uint64_t seek_ms)
{
    publish_int32_event (guild_id, SOCKET_BINARY_EVENT_SEEK, seek_ms);
}

void
publish_stop (const dpp::snowflake &guild_id)
{
    publish_event_binary (guild_id, SOCKET_BINARY_EVENT_STOP);
}

void
publish_fx (const dpp::snowflake &guild_id)
{
    auto guild_player = ::musicat::player::manager::get_player (guild_id);
    if (!guild_player)
        return;

    publish_event (guild_id, SOCKET_EVENT_FX, guild_player->fx_states_to_json ());
}

void
publish_queue (const dpp::snowflake &guild_id)
{
    publish_event (guild_id, SOCKET_EVENT_QUEUE, get_queue_payload (guild_id));
}

void
publish_progress (const dpp::snowflake &guild_id, const uint64_t ms)
{
    publish_int32_event (guild_id, SOCKET_BINARY_EVENT_PROGRESS, ms);
}

////////////////////////////////////////////////////////////////////////////////

void
send_error (const dpp::snowflake &user_id, const nlohmann::json &err)
{
    send_event (user_id, SOCKET_EVENT_ERROR, { { "error", true }, { "error_message", err } });
}

void
send_join (const dpp::snowflake &user_id)
{
    send_event_binary (user_id, SOCKET_BINARY_EVENT_JOINVC);
}

void
send_leave (const dpp::snowflake &user_id)
{
    send_event_binary (user_id, SOCKET_BINARY_EVENT_LEAVEVC);
}

////////////////////////////////////////////////////////////////////////////////

} // musicat::server::ws::player
