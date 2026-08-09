#include "musicat/player_manager.h"

#include "musicat/events/on_voice_state_update.h"
#include "musicat/musicat.h"

namespace musicat::events
{
void
on_voice_state_update (dpp::cluster *client)
{
    client->on_voice_state_update (
        [] (const dpp::voice_state_update_t &event)
            {
                const bool debug = get_debug_state ();
                if (debug)
                    std::cerr << "[events::on_voice_state_update]: " << event.raw_event << "\n";

                player::manager::handle_on_voice_state_update (event);
            });
}
} // musicat::events
