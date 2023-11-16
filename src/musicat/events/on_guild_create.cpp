#include "musicat/events/on_guild_create.h"
#include "musicat/server/service_cache.h"

namespace musicat::events
{

void
on_guild_create (dpp::cluster *client)
{
    client->on_guild_create ([] (const dpp::guild_create_t &e) {
        server::service_cache::handle_guild_create (e);
    });
}

} // musicat::events
