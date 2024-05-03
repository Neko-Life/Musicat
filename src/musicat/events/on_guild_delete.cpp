#include "musicat/events/on_guild_delete.h"
#include "musicat/server/service_cache.h"

namespace musicat::events
{

void
on_guild_delete (dpp::cluster *client)
{
#ifndef MUSICAT_NO_SERVER
    client->on_guild_delete ([] (const dpp::guild_delete_t &e) {
        server::service_cache::handle_guild_delete (e);
    });
#endif
}

} // musicat::events
