#include "musicat/cmds/skip.h"
#include "musicat/cmds.h"
#include "musicat/musicat.h"
#include "musicat/util.h"
#include "musicat/util_response.h"

namespace musicat::command::skip
{
dpp::slashcommand
get_register_obj (const dpp::snowflake &sha_id)
{
    return dpp::slashcommand ("skip", "Skip [currently playing] song", sha_id)
        .add_option (dpp::command_option (dpp::co_integer, "amount",
                                          "How many [song] to skip"))
        .add_option (
            create_yes_no_option ("remove", "Remove [song] while skipping"));
}

void
slash_run (const dpp::slashcommand_t &event)
{
    auto player_manager = get_player_manager_ptr ();
    if (!player_manager)
        {
            return;
        }

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

            auto res = player_manager->skip (v, event.command.guild_id,
                                             event.command.usr.id, am,
                                             rm ? true : false);
            switch (res.second)
                {
                case 0:
                    event.reply (
                        util::response::reply_skipped_track (res.first));
                    break;
                case -1:
                    event.reply ("I'm not playing anything");
                    break;
                default:
                    event.reply (std::to_string (res.second)
                                 + util::join (res.second > 1, " member", "s")
                                 + " voted to skip, add more vote to skip "
                                   "current track");
                }
        }

    catch (const exception &e)
        {
            event.reply (e.what ());
        }
}
} // musicat::command::skip

// vim: et sw=4 ts=8
