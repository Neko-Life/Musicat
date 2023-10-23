#include "musicat/events/on_voice_state_update.h"
#include "musicat/musicat.h"

namespace musicat::events
{
void
on_voice_state_update (dpp::cluster *client)
{
    client->on_voice_state_update (
        [] (const dpp::voice_state_update_t &event) {
            auto player_manager = get_player_manager_ptr ();

            if (player_manager)
                player_manager->handle_on_voice_state_update (event);
        });
}
} // musicat::events
