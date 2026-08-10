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

struct SocketData
{
    dpp::snowflake server_id;
    dpp::snowflake user_id;
    bool waved;

    SocketData ();
    SocketData (const dpp::snowflake &_server_id);

    std::string get_player_topic () const;
};

enum socket_event_e
{
    SOCKET_EVENT_ERROR,
    SOCKET_EVENT_PAUSE,
    SOCKET_EVENT_PLAYBACK_INFO,
    SOCKET_EVENT_PLAY,
    SOCKET_EVENT_SEEK,
    SOCKET_EVENT_STOP,
    SOCKET_EVENT_FX,
    SOCKET_EVENT_QUEUE,
    SOCKET_EVENT_REGISTER,
    SOCKET_EVENT_NEXT,
    SOCKET_EVENT_PREV,
    SOCKET_EVENT_JOINVC,
    SOCKET_EVENT_LEAVEVC,
    SOCKET_EVENT_PROGRESS
};

// clang-format off
enum socket_binary_event_e : char {
	SOCKET_BINARY_EVENT_ERROR           = 'e',
	SOCKET_BINARY_EVENT_PAUSE           = 'p',
	SOCKET_BINARY_EVENT_PLAYBACK_INFO   = 'P',
	SOCKET_BINARY_EVENT_PLAY            = 'c',
	SOCKET_BINARY_EVENT_SEEK            = 's',
	SOCKET_BINARY_EVENT_STOP            = 'x',
	SOCKET_BINARY_EVENT_FX              = 'f',
	SOCKET_BINARY_EVENT_QUEUE           = 'q',
	SOCKET_BINARY_EVENT_REGISTER        = 'r',
	SOCKET_BINARY_EVENT_NEXT            = 'n',
	SOCKET_BINARY_EVENT_PREV            = 'z',
	SOCKET_BINARY_EVENT_JOINVC          = 'j',
	SOCKET_BINARY_EVENT_LEAVEVC         = 'l',
	SOCKET_BINARY_EVENT_PROGRESS        = 'S'
};
// clang-format on

using uws_ws_t = uWS::WebSocket<SERVER_WITH_SSL, true, SocketData>;

struct socket_event_handler_t
{
    const socket_event_e event;
    int (*const handler) (const nlohmann::json &data, uws_ws_t *ws);
};

int register_ws_user (uws_ws_t *ws, const dpp::snowflake &user_id);
uws_ws_t *get_user_ws (const dpp::snowflake &user_id);
int unregister_ws_user (uws_ws_t *ws);

////////////////////////////////////////////////////////////////////////////////

std::string get_player_topic (const dpp::snowflake &guild_id);

APIApp::WebSocketBehavior<SocketData> get_behavior ();

////////////////////////////////////////////////////////////////////////////////

nlohmann::json get_queue_payload (const dpp::snowflake &guild_id);

void publish_event (const dpp::snowflake &guild_id, const socket_event_e event, const nlohmann::json &data);
void publish_event_binary (const dpp::snowflake &guild_id, const socket_binary_event_e event, const std::string &data);
void send_event (const dpp::snowflake &user_id, const socket_event_e event, const nlohmann::json &data);
void send_event_binary (const dpp::snowflake &user_id, const socket_binary_event_e event, const nlohmann::json &data);

void publish_error (const dpp::snowflake &guild_id, const nlohmann::json &err);
void publish_pause (const dpp::snowflake &guild_id);
void publish_playback_info (const dpp::snowflake &guild_id);
void publish_play (const dpp::snowflake &guild_id);
void publish_seek (const dpp::snowflake &guild_id, const uint64_t seek_ms);
void publish_stop (const dpp::snowflake &guild_id);
void publish_fx (const dpp::snowflake &guild_id);
void publish_queue (const dpp::snowflake &guild_id);
void publish_progress (const dpp::snowflake &guild_id, const uint64_t ms);

void send_error (const dpp::snowflake &user_id, const nlohmann::json &err);
void send_join (const dpp::snowflake &user_id);
void send_leave (const dpp::snowflake &user_id);

/*
nlohmann::json create_error_data (const socket_err_code_e code,
                                  const std::string &message,
                                  const http_status_code_e status);

// utils to create json payload
nlohmann::json create (const socket_event_e event, const nlohmann::json &data);
*/
} // musicat::server::ws::player

#endif // MUSICAT_SERVER_WS_PLAYER_H
