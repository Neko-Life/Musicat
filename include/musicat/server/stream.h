#ifndef MUSICAT_SERVER_STREAM_H
#define MUSICAT_SERVER_STREAM_H

#include "musicat/server.h"
#include <dpp/dpp.h>

namespace musicat::server::stream
{

void subscribe (APIResponse *res, const dpp::snowflake &guild_id);

void unsubscribe (APIResponse *res, const dpp::snowflake &guild_id, bool aborted);
void unsubscribe (const dpp::snowflake &guild_id);

void shutdown ();

void broadcast (const dpp::snowflake &guild_id, const unsigned char *packet, size_t packet_len);

} // musicat::server::stream

#endif // MUSICAT_SERVER_STREAM_H
