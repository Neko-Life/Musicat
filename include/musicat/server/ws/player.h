#ifndef MUSICAT_SERVER_WS_PLAYER_H
#define MUSICAT_SERVER_WS_PLAYER_H

#include "musicat/server.h"
#include "nlohmann/json.hpp"
#include <dpp/dpp.h>

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
*/

namespace musicat::server::ws::player
{

#ifndef MUSICAT_NO_SERVER
struct SocketData
{
    dpp::snowflake server_id;
};

enum socket_event_e
{
    SOCKET_EVENT_ERROR,
    SOCKET_EVENT_PAUSE,
};

struct socket_event_handler_t
{
    const socket_event_e event;
    void (*const handler) (const nlohmann::json &payload);
};

using uws_ws_t = uWS::WebSocket<SERVER_WITH_SSL, true, SocketData>;

uWS::TemplatedApp<SERVER_WITH_SSL>::WebSocketBehavior<SocketData>
get_behavior ();

/*
nlohmann::json create_error_data (const socket_err_code_e code,
                                  const std::string &message,
                                  const http_status_code_e status);

// utils to create json payload
nlohmann::json create (const socket_event_e event, const nlohmann::json &data);
*/
#endif
} // musicat::server::ws::player

#endif // MUSICAT_SERVER_WS_PLAYER_H
