#include "musicat/events/on_voice_server_update.h"
#include "musicat/events.h"
#include "musicat/musicat.h"

namespace musicat::events
{
void
on_voice_server_update (dpp::cluster *client)
{
#ifdef USE_VOICE_SERVER_UPDATE_RECONNECT
    client->on_voice_server_update (
        [] (const dpp::voice_server_update_t &event) {
            auto player_manager = get_player_manager_ptr ();
            if (!player_manager
                || player_manager->is_waiting_vc_ready (event.guild_id))
                return;

            if (auto guild_player
                = player_manager->get_player (event.guild_id);
                guild_player)
                guild_player->check_for_to_seek ();

            dpp::voiceconn *connection
                = event.from->get_voice (event.guild_id);

            if (!connection || !connection->is_active ())
                return;

            if (get_debug_state ())
                fprintf (stderr,
                         "[events::on_voice_server_update] "
                         "Reconnecting to new Voice Server (%s)\n",
                         event.guild_id.str ().c_str ());

            player_manager->set_waiting_vc_ready (event.guild_id);

            connection->disconnect ();
            connection->websocket_hostname = event.endpoint;
            connection->connect (event.guild_id);
        });
#endif
}
} // musicat::events
