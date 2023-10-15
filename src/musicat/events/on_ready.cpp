#include "musicat/events/on_ready.h"

namespace musicat::events
{
void
on_ready (dpp::cluster *client)
{
    client->on_ready ([] (const dpp::ready_t &event) {
        dpp::discord_client *from = event.from;
        dpp::user me = from->creator->me;

        fprintf (stderr, "[READY] Shard: %d\n", from->shard_id);
        std::cerr << "Logged in as " << me.username << '#' << me.discriminator
                  << " (" << me.id << ")\n";
    });
}
} // musicat::events
