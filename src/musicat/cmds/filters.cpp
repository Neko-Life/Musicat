#include "musicat/cmds.h"
#include "musicat/musicat.h"
#include "musicat/util.h"

namespace musicat
{
namespace command
{
// save, load, delete filters manage subcommand
namespace filters
{
dpp::slashcommand
get_register_obj (const dpp::snowflake &sha_id)
{
    dpp::slashcommand slash ("filters", "Playback filters", sha_id);

    // setup command
    // setup_register_obj (slash);

    return slash;
}

void
slash_run (const dpp::slashcommand_t &event)
{
    // perquisite
    player::player_manager_ptr player_manager = get_player_manager_ptr ();

    if (!player_manager)
        {
            return;
        }

    if (!player_manager->voice_ready (event.command.guild_id, event.from,
                                      event.command.usr.id))
        {
            event.reply ("Please wait while I'm getting ready to stream");
            return;
        }

    auto player = player_manager->get_player (event.command.guild_id);

    if (!player)
        {
            event.reply ("No active player in this guild");
            return;
        }
    auto guild_player = player_manager->get_player (event.command.guild_id);

    // run subcommand
    //
    //
}

} // filters
} // command
} // musicat
