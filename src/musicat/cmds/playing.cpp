// clang-format off
#include "musicat/player.h"
// clang-format on

#include "musicat/cmds/playing.h"
#include "musicat/cmds.h"

namespace musicat::command::playing
{
dpp::slashcommand
get_register_obj (const dpp::snowflake &sha_id)
{
    return dpp::slashcommand ("playing", "Current playback info", sha_id);
}

void
slash_run (const dpp::slashcommand_t &event)
{
    if (!cmd_pre_get_player_manager_ready (event))
        return;

    auto guild_player = player::manager::create_player (event.command.guild_id);
    if (!guild_player)
        return event.reply ("`[ERROR]` Failed creating guild player");

    if (guild_player->current_track.is_empty ())
        {
            return event.reply ("Nothing is playing right now, try the `/play` command");
        }

    player::manager::reply_info_embed (event, false);
}

} // musicat::command::playing
