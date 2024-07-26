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
    auto player_manager = cmd_pre_get_player_manager_ready (event);
    if (player_manager == NULL)
        return;

    auto guild_player = player_manager->create_player (event.command.guild_id);
    guild_player->from = event.from;

    if (guild_player->current_track.is_empty ())
        {
            return event.reply (
                "Nothing is playing right now, try the `/play` command");
        }

    player_manager->reply_info_embed (event);
}

} // musicat::command::playing
