#include "musicat/cmds.h"
#include "musicat/util.h"

namespace musicat
{
namespace command
{
namespace skip
{
dpp::slashcommand
get_register_obj (const dpp::snowflake &sha_id)
{
    return dpp::slashcommand ("skip", "Skip [currently playing] song", sha_id)
        .add_option (dpp::command_option (dpp::co_integer, "amount",
                                          "How many [song] to skip"))
        .add_option (dpp::command_option (dpp::co_integer, "remove",
                                          "Remove [song] while skipping")
                         .add_choice (dpp::command_option_choice ("Yes", 1))
                         .add_choice (dpp::command_option_choice ("No", 0)));
}

void
slash_run (const dpp::slashcommand_t &event,
           player::player_manager_ptr player_manager)
{
    if (!player_manager->voice_ready (event.command.guild_id))
        {
            event.reply ("Please wait while I'm getting ready to stream");
            return;
        }

    try
        {
            auto v = event.from->get_voice (event.command.guild_id);
            int64_t am = 1;
            int64_t rm = 0;
            get_inter_param (event, "amount", &am);
            get_inter_param (event, "remove", &rm);
            int res = player_manager->skip (v, event.command.guild_id,
                                            event.command.usr.id, am,
                                            rm ? true : false);
            switch (res)
                {
                case 0:
                    event.reply ("Skipped");
                    break;
                case -1:
                    event.reply ("I'm not playing anything");
                    break;
                default:
                    event.reply (std::to_string (res)
                                 + util::join (res > 1, " member", "s")
                                 + " voted to skip, add more vote to skip "
                                   "current track");
                }
        }

    catch (const exception &e)
        {
            event.reply (e.what ());
        }
}
} // skip
} // command
} // musicat

// vim: et sw=4 ts=8
