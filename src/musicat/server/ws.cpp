#include "musicat/server/ws.h"

namespace musicat::server::ws
{
#ifndef MUSICAT_NO_SERVER

// inline constexpr const ws_handler_t ws_handlers[] = {};

void
define_ws_routes (APIApp *app)
{
    app->ws<player::SocketData> ("/ws/player/:server_id",
                                 ws::player::get_behavior ());
}

#endif
} // musicat::server::ws
