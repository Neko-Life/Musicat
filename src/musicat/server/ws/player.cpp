#include "musicat/musicat.h"
#include "musicat/server.h"
#include "musicat/server/ws/player_events.h"
#include <uWebSockets/src/App.h>

namespace musicat::server::ws::player
{

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

void
publish_player_info (const dpp::snowflake &guild_id)
{
    nlohmann::json data = util::get_playback_info_json (guild_id);

    nlohmann::json d = nlohmann::json::object ({ { "e", SOCKET_EVENT_PLAYBACK_INFO }, { "d", data } });
    server::publish ("player/" + guild_id.str (), d.dump ());
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
