#include "musicat/cmds.h"

namespace musicat
{
namespace command
{
namespace move
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

    const bool debug = get_debug_state ();

    if (debug)
        fprintf (stderr, "[move::slash_run] Locked player::t_mutex: %ld\n",
                 guild_player->guild_id);

    std::lock_guard<std::mutex> lk (guild_player->t_mutex);

    size_t queue_siz = guild_player->queue.size ();
    int64_t max_to = (int64_t)queue_siz - 1;
    if (2 > max_to)
        {
            event.reply ("No moveable track in the queue");
            if (debug)
                fprintf (
                    stderr,
                    "[move::slash_run] Should unlock player::t_mutex: %ld\n",
                    guild_player->guild_id);
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
                a = guild_player->queue.at (1).title ();
                b = guild_player->queue.back ().title ();

                guild_player->queue.erase (guild_player->queue.begin () + fr);
                guild_player->queue.insert (guild_player->queue.begin () + to,
                                            track);
            }

            if (a != guild_player->queue.at (1).title ()
                || b != guild_player->queue.back ().title ())
                try
                    {
                        player_manager->update_info_embed (
                            event.command.guild_id);
                    }
                catch (...)
                    {
                    }
        }

    event.reply (std::string ("Moved ") + track.title () + " to position "
                 + std::to_string (to));

    if (debug)
        fprintf (stderr,
                 "[move::slash_run] Should unlock player::t_mutex: %ld\n",
                 guild_player->guild_id);
}
} // move
} // command
} // musicat
