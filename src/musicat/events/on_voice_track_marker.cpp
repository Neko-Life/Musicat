#include "musicat/player_manager.h"

#include "musicat/events/on_voice_track_marker.h"
// #include "musicat/player_manager_timer.h"

namespace musicat::events
{
void
on_voice_track_marker (dpp::cluster *client)
{
    client->on_voice_track_marker (
        [] (const dpp::voice_track_marker_t &event)
            {
                player::manager::handle_on_track_marker (event);

                // dispatch ignore marker remover
                // player::timer::create_track_marker_rm_timer (event.track_meta,
                //                                              event.voice_client);
            });
}
} // musicat::events
