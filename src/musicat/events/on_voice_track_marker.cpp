#include "musicat/events/on_voice_track_marker.h"
#include "musicat/musicat.h"
#include "musicat/player_manager_timer.h"
#include "musicat/thread_manager.h"

namespace musicat::events
{
void
on_voice_track_marker (dpp::cluster *client)
{
    client->on_voice_track_marker (
        [] (const dpp::voice_track_marker_t &event) {
            // bool debug = get_debug_state ();
            auto player_manager = get_player_manager_ptr ();

            if (!player_manager)
                return;

            // if (player_manager->has_ignore_marker
            // (event.voice_client->server_id))
            //     {
            //         if (debug)
            //             std::cerr << "[PLAYER_MANAGER] Meta \"" <<
            //             event.track_meta
            //                       << "\" is ignored in guild "
            //                       << event.voice_client->server_id << "\n";

            //         return;
            //     }

            player_manager->handle_on_track_marker (event);

            // if (event.track_meta != "rm")
            //     player_manager->set_ignore_marker
            //     (event.voice_client->server_id);

            // if (!player_manager->handle_on_track_marker (event))
            //     player_manager->delete_info_embed
            //     (event.voice_client->server_id);

            // dispatch ignore marker remover
            // player::timer::create_track_marker_rm_timer (event.track_meta,
            //                                              event.voice_client);
        });
}
} // musicat::events
