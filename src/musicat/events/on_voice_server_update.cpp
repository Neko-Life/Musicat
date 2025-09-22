#include "musicat/events/on_voice_server_update.h"
#include "musicat/events.h"
#include "musicat/musicat.h"
#include "musicat/util.h"
#include <stdio.h>

namespace musicat::events
{

void
ret_pline (void *status)
{
    int *s = (int *)status;

    switch (*s)
        {
        case 1:
            fprintf (stderr, "[WAITING_VC_READY]");
            break;
        case 2:
            fprintf (stderr, "[NO_CONN]");
            break;
        case 3:
            fprintf (stderr, "[NO_ACTIVE_CONN]");
            break;
        case 4:
            fprintf (stderr, "[NO_OP]");
            break;
        }

    fprintf (stderr, "\n");
}

void
on_voice_server_update (dpp::cluster *client)
{
#ifdef USE_VOICE_SERVER_UPDATE_RECONNECT
    client->on_voice_server_update (
        [] (const dpp::voice_server_update_t &event) {
            const bool debug = get_debug_state ();

            if (debug)
                fprintf (stderr,
                         "[events::on_voice_server_update] "
                         "%s: %s <- ",
                         event.guild_id.str ().c_str (),
                         event.endpoint.c_str ());

            int status = 0;
            util::ret_hook_t h{ &status, ret_pline };

            auto player_manager = get_player_manager_ptr ();
            if (!player_manager)
                return;

            if (auto guild_player
                = player_manager->get_player (event.guild_id);
                guild_player)
                guild_player->check_for_to_seek ();

            if (player_manager->is_waiting_vc_ready (event.guild_id))
                {
                    status = 1;
                    return;
                }

            dpp::voiceconn *connection
                = event.from ()->get_voice (event.guild_id);

            if (!connection)
                {
                    status = 2;
                    return;
                }

            fprintf (stderr, "%s ", connection->websocket_hostname.c_str ());

            if (!connection->is_active ())
                {
                    status = 3;
                    return;
                }

            if (connection->websocket_hostname == event.endpoint)
                {
                    status = 4;
                    return;
                }

            // reconnect voice
            player_manager->full_reconnect (event.from (), event.guild_id,
                                            connection->channel_id,
                                            connection->channel_id);
        });
#endif
}
} // musicat::events
