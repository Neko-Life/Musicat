#include "musicat/cmds/remove.h"
#include "musicat/mctrack.h"
#include "musicat/musicat.h"
#include "musicat/util.h"

namespace musicat::command::remove
{
dpp::slashcommand
get_register_obj (const dpp::snowflake &sha_id)
{
    return dpp::slashcommand ("remove", "Remove [tracks] from the queue",
                              sha_id)
        .add_option (dpp::command_option (dpp::co_integer, "track",
                                          "Remove [this] track", true))
        .add_option (dpp::command_option (
            dpp::co_integer, "amount",
            "Amount [of track] to remove [starting from this track]"))
        .add_option (dpp::command_option (
            dpp::co_integer, "to", "Remove [up to this] track [position]"));
}

void
slash_run (const dpp::slashcommand_t &event)
{
    auto player_manager = get_player_manager_ptr ();
    if (!player_manager)
        {
            return;
        }

    int64_t fr = 1;
    int64_t amount = 1;
    int64_t to = -1;
    get_inter_param (event, "track", &fr);
    get_inter_param (event, "amount", &amount);
    get_inter_param (event, "to", &to);

    std::deque<player::MCTrack> queue
        = player_manager->get_queue (event.command.guild_id);
    if (fr < 1 || ((size_t)fr + 1) > queue.size ())
        {
            event.reply ("No track in position " + std::to_string (fr));
            return;
        }

    if (to != -1 && to < fr)
        {
            event.reply ("Invalid `to` argument. Must be larger than `track`");
            return;
        }

    event.thinking ();

    if (amount < 1)
        amount = 1;

    player::MCTrack f_t = queue.at (1);
    player::MCTrack l_t = queue.back ();

    const size_t ret = player_manager->remove_track (
        event.command.guild_id, (size_t)fr, (size_t)amount, (size_t)to);
    if (ret)
        {
            event.edit_response ("Removed " + std::to_string (ret)
                                 + util::join (ret != 1, " track", "s"));

            auto queue2 = player_manager->get_queue (event.command.guild_id);
            if (queue2.size () < 2U
                || mctrack::get_title (queue2.at (1))
                       != mctrack::get_title (f_t)
                || mctrack::get_title (queue2.back ())
                       != mctrack::get_title (l_t))
                try
                    {
                        player_manager->update_info_embed (
                            event.command.guild_id);
                    }
                catch (...)
                    {
                    }
        }
    else
        {
            event.edit_response ("No track was removed");
        }
}
} // musicat::command::remove
