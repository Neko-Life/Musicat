#include "musicat/player_manager.h"

#include "musicat/events/on_voice_ready.h"

namespace musicat::events
{
void
on_voice_ready (dpp::cluster *client)
{
    client->on_voice_ready ([] (const dpp::voice_ready_t &event) { player::manager::handle_on_voice_ready (event); });
}
} // musicat::events
