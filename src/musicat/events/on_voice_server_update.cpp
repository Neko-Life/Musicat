#include "musicat/events/on_voice_server_update.h"

namespace musicat::events
{
void
on_voice_server_update (dpp::cluster *client)
{
    client->on_voice_server_update (
        [] (const dpp::voice_server_update_t &event) {
            dpp::voiceconn *connection
                = event.from->get_voice (event.guild_id);
            if (!connection
                || connection->websocket_hostname == event.endpoint)
                return;
            connection->disconnect ();
            connection->websocket_hostname = event.endpoint;
            connection->connect (event.guild_id);
        });
}
} // musicat::events
