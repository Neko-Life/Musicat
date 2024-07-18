#include "musicat/cmds/pause.h"
#include "musicat/cmds.h"
#include "musicat/musicat.h"

namespace musicat::command::pause
{
dpp::slashcommand
get_register_obj (const dpp::snowflake &sha_id)
{
    return dpp::slashcommand ("pause", "Pause [currently playing] song",
                              sha_id);
}

void
slash_run (const dpp::slashcommand_t &event)
{
    auto player_manager = cmd_pre_get_player_manager_ready (event);
    if (player_manager == NULL)
        return;

    try
        {
            if (player_manager->pause (event.from, event.command.guild_id,
                                       event.command.usr.id))
                event.reply ("Paused");
            else
                event.reply ("I'm not playing anything");
        }
    catch (const exception &e)
        {
            event.reply (e.what ());
        }
}
} // musicat::command::pause
