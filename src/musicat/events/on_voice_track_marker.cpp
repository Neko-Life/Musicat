#include "musicat/events/on_voice_track_marker.h"
#include "musicat/musicat.h"
#include "musicat/player_manager_timer.h"

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

            player_manager->handle_on_track_marker (event);

            // dispatch ignore marker remover
            // player::timer::create_track_marker_rm_timer (event.track_meta,
            //                                              event.voice_client);
        });
}
} // musicat::events
