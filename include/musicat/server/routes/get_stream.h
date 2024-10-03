#ifndef MUSICAT_SERVER_ROUTES_GET_STREAM_H
#define MUSICAT_SERVER_ROUTES_GET_STREAM_H

#include "musicat/server.h"
#include <dpp/dpp.h>

namespace musicat::server::routes
{

void send_to_all_streaming_state (const dpp::snowflake &guild_id,
                                  uint8_t *packet, int len);

void get_stream (APIResponse *res, APIRequest *req);

} // musicat::server::routes

#endif // MUSICAT_SERVER_ROUTES_GET_STREAM_H
