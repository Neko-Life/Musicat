#include "musicat/cmds.h"
#include "musicat/musicat.h"
#include "musicat/player.h"

namespace musicat
{
namespace command
{
namespace pause
{
dpp::slashcommand
get_register_obj (const dpp::snowflake &sha_id)
{
    return dpp::slashcommand ("pause", "Pause [currently playing] song",
                              sha_id);
}

void
slash_run (const dpp::interaction_create_t &event,
           player::player_manager_ptr player_manager)
{
    if (!player_manager->voice_ready (event.command.guild_id))
        {
            event.reply ("Please wait while I'm getting ready to stream");
            return;
        }
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
} // pause
} // command
} // musicat
