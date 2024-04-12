#include "musicat/musicat.h"
#include "musicat/server.h"
#include "musicat/server/ws/player_events.h"
#include <uWebSockets/src/App.h>

namespace musicat::server::ws::player
{

#ifndef MUSICAT_NO_SERVER
uWS::TemplatedApp<SERVER_WITH_SSL>::WebSocketBehavior<SocketData>
get_behavior ()
{
    // use default uws options outside handlers
    uWS::TemplatedApp<SERVER_WITH_SSL>::WebSocketBehavior<SocketData> b;

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

#endif
} // musicat::server::ws::player
