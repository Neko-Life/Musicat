#ifndef MUSICAT_SERVER_WS_H
#define MUSICAT_SERVER_WS_H

#include "musicat/server.h"
#include "musicat/server/ws/player.h"

namespace musicat::server::ws
{

#ifndef MUSICAT_NO_SERVER
void define_ws_routes (APIApp *app);
#endif

} // musicat::server::ws

#endif // MUSICAT_SERVER_WS_H
