#include "musicat/events/on_voice_track_marker.h"
#include "musicat/musicat.h"
#include "musicat/thread_manager.h"

namespace musicat::events
{
void
on_voice_track_marker (dpp::cluster *client)
{
    client->on_voice_track_marker ([] (const dpp::voice_track_marker_t
                                           &event) {
        bool debug = get_debug_state ();
        auto player_manager = get_player_manager_ptr ();

        if (!player_manager)
            return;

        if (player_manager->has_ignore_marker (event.voice_client->server_id))
            {
                if (debug)
                    std::cerr << "[PLAYER_MANAGER] Meta \"" << event.track_meta
                              << "\" is ignored in guild "
                              << event.voice_client->server_id << "\n";

                return;
            }

        if (event.track_meta != "rm")
            player_manager->set_ignore_marker (event.voice_client->server_id);

        if (!player_manager->handle_on_track_marker (event))
            player_manager->delete_info_embed (event.voice_client->server_id);

        // ignore marker remover
        std::thread t ([event] () {
            thread_manager::DoneSetter tmds;

            auto player_manager = get_player_manager_ptr ();

            bool debug = get_debug_state ();
            short int count = 0;
            int until_count;
            bool run_state = false;

            auto player
                = ((run_state = get_running_state ()) && player_manager)
                      ? player_manager->get_player (
                          event.voice_client->server_id)
                      : NULL;

            if (!event.voice_client || event.voice_client->terminating
                || !player)
                goto marker_remover_end;

            until_count = player->saved_queue_loaded ? 10 : 30;
            while ((run_state = get_running_state ()) && player
                   && player_manager && event.voice_client
                   && !event.voice_client->terminating
                   && !event.voice_client->is_playing ()
                   && !event.voice_client->is_paused ())
                {
                    std::this_thread::sleep_for (
                        std::chrono::milliseconds (500));

                    count++;

                    if (count == until_count)
                        break;
                }

            if (!(run_state = get_running_state ()) || !event.voice_client
                || event.voice_client->terminating || !player_manager
                || !player)
                {
                    return;
                }

        marker_remover_end:
            player_manager->remove_ignore_marker (
                event.voice_client->server_id);

            if (!debug)
                {
                    return;
                }

            fprintf (stderr, "Removed ignore marker for meta '%s'",
                     event.track_meta.c_str ());

            if (event.voice_client)
                std::cerr << " in " << event.voice_client->server_id;

            fprintf (stderr, "\n");
        });

        thread_manager::dispatch (t);
    });
}
} // musicat::events
