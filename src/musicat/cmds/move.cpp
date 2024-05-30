#include "musicat/cmds/move.h"
#include "musicat/mctrack.h"
#include "musicat/musicat.h"

namespace musicat::command::move
{
dpp::slashcommand
get_register_obj (const dpp::snowflake &sha_id)
{
    return dpp::slashcommand ("move", "Move [track position] around the queue",
                              sha_id)
        .add_option (dpp::command_option (dpp::co_integer, "track",
                                          "Move [this] track", true))
        .add_option (dpp::command_option (dpp::co_integer, "to",
                                          "Move [to this] position", true));
}

void
slash_run (const dpp::slashcommand_t &event)
{
    auto player_manager = get_player_manager_ptr ();
    if (!player_manager)
        {
            return;
        }

    auto guild_player = player_manager->get_player (event.command.guild_id);
    if (!guild_player)
        {
            event.reply ("No track");
            return;
        }

    std::lock_guard lk (guild_player->t_mutex);

    size_t queue_siz = guild_player->queue.size ();
    int64_t max_to = (int64_t)queue_siz - 1;
    if (2 > max_to)
        {
            event.reply ("No moveable track in the queue");

            return;
        }

    int64_t fr = 0;
    int64_t to = 0;
    get_inter_param (event, "track", &fr);
    get_inter_param (event, "to", &to);

    if (fr < 1)
        fr = 1;
    if (to < 1)
        to = 1;
    if (fr > max_to)
        fr = max_to;
    if (to > max_to)
        to = max_to;

    // copy because it will be erased
    player::MCTrack track = guild_player->queue.at (fr);

    if (fr != to)
        {
            std::string a;
            std::string b;
            {
                a = mctrack::get_title (guild_player->queue.at (1));
                b = mctrack::get_title (guild_player->queue.back ());

                guild_player->queue.erase (guild_player->queue.begin () + fr);
                guild_player->queue.insert (guild_player->queue.begin () + to,
                                            track);
            }

            if (a != mctrack::get_title (guild_player->queue.at (1))
                || b != mctrack::get_title (guild_player->queue.back ()))
                try
                    {
                        player_manager->update_info_embed (
                            event.command.guild_id);
                    }
                catch (...)
                    {
                    }
        }

    event.reply (std::string ("Moved ") + mctrack::get_title (track)
                 + " to position " + std::to_string (to));
}
} // musicat::command::move
