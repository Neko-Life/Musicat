#ifndef MUSICAT_SERVER_WS_PLAYER_EVENTS_H
#define MUSICAT_SERVER_WS_PLAYER_EVENTS_H

#include "musicat/server.h"
#include "musicat/server/ws/player.h"

namespace musicat::server::ws::player::events
{

void upgrade (uWS::HttpResponse<SERVER_WITH_SSL> *res, uWS::HttpRequest *req,
              struct us_socket_context_t *sock);

void open (uws_ws_t *ws);

void message (uws_ws_t *ws, std::string_view msg, uWS::OpCode code);

void drain (uws_ws_t *ws);

void ping (uws_ws_t *ws, std::string_view msg);

void pong (uws_ws_t *ws, std::string_view msg);

void subscription (uws_ws_t *ws, std::string_view topic, int idk1, int idk2);

void close (uws_ws_t *ws, int code, std::string_view message);

} // musicat::server::ws::player::events

#endif // MUSICAT_SERVER_WS_PLAYER_EVENTS_H
