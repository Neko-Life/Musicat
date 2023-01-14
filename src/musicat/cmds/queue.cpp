#include "musicat/cmds.h"
#include "musicat/pagination.h"

namespace musicat
{
namespace command
{
namespace queue
{
dpp::slashcommand
get_register_obj (const dpp::snowflake &sha_id)
{
    return dpp::slashcommand ("queue", "Show or modify [tracks in the] queue",
                              sha_id)
        .add_option (
            dpp::command_option (dpp::co_integer, "action",
                                 "Modify [tracks in the] queue")
                .add_choice (dpp::command_option_choice (
                    "Shuffle", queue_modify_t::m_shuffle))
                .add_choice (dpp::command_option_choice (
                    "Reverse", queue_modify_t::m_reverse))
                .add_choice (dpp::command_option_choice (
                    "Clear Left", queue_modify_t::m_clear_left))
                .add_choice (dpp::command_option_choice (
                    "Clear All", queue_modify_t::m_clear))
                .add_choice (dpp::command_option_choice (
                    "Clear Musicat", queue_modify_t::m_clear_musicat)));
}

void
slash_run (const dpp::interaction_create_t &event,
           player::player_manager_ptr player_manager)
{
    const dpp::snowflake shaid = event.from->creator->me.id;
    player_manager->load_guild_current_queue (event.command.guild_id, &shaid);

    std::deque<player::MCTrack> queue
        = player_manager->get_queue (event.command.guild_id);

    if (queue.empty ())
        {
            event.reply ("No track");
            return;
        }

    int64_t qarg = -1;
    get_inter_param (event, "action", &qarg);

    if (qarg > -1)
        {
            switch (qarg)
                {
                case queue_modify_t::m_clear:
                    {
                        auto siz = queue.size ();
                        if (siz < 2)
                            {
                                event.reply (
                                    "No track to clear, skip the current "
                                    "track to clear the queue completely");
                                break;
                            }
                        auto p = player_manager->get_player (
                            event.command.guild_id);
                        p->reset_shifted ();

                        {
                            std::lock_guard<std::mutex> lk (p->q_m);
                            player::MCTrack t = p->queue.at (0);
                            p->queue.clear ();
                            p->queue.push_back (t);
                        }

                        try
                            {
                                player_manager->update_info_embed (
                                    event.command.guild_id);
                            }
                        catch (...)
                            {
                            }
                        event.reply ("Cleared");

                        break;
                    }
                case queue_modify_t::m_shuffle:
                    {
                        if (player_manager->shuffle_queue (
                                event.command.guild_id))
                            {
                                event.reply (
                                    "Shuffled"
                                    + std::string (
                                        queue.size () < 5
                                            ? ", I suggest the `/move` "
                                              "command if you do think my "
                                              "shuffling skill isn't very "
                                              "much satisfying"
                                            : ""));
                                break;
                            }
                        else
                            {
                                event.reply ("Doesn't seem like shuffleable");
                                break;
                            }
                    }
                case queue_modify_t::m_reverse:
                    {
                        auto siz = queue.size ();
                        if (siz < 3)
                            {
                                event.reply ("Reversed. Not that you can tell "
                                             "the difference");
                                break;
                            }
                        auto p = player_manager->get_player (
                            event.command.guild_id);
                        p->reset_shifted ();
                        {
                            std::lock_guard<std::mutex> lk (p->q_m);
                            player::MCTrack t = p->queue.at (0);

                            std::deque<player::MCTrack> n_queue = {};
                            for (size_t i = (siz - 1); i > 0; i--)
                                n_queue.push_back (p->queue.at (i));

                            p->queue.clear ();
                            p->queue = n_queue;
                            p->queue.push_front (t);
                        }
                        player_manager->update_info_embed (
                            event.command.guild_id);
                        event.reply ("Reversed");

                        break;
                    }
                case queue_modify_t::m_clear_left:
                    {
                        if (!player_manager->voice_ready (
                                event.command.guild_id, event.from,
                                event.command.usr.id))
                            {
                                event.reply (
                                    "Establishing connection. Please wait...");
                                break;
                            }
                        size_t rmed = 0;
                        std::vector<dpp::snowflake> l_user = {};

                        auto p = player_manager->get_player (
                            event.command.guild_id);
                        try
                            {
                                auto vc = get_voice_from_gid (
                                    event.command.guild_id,
                                    player_manager->sha_id);
                                {
                                    std::lock_guard<std::mutex> lk (p->q_m);
                                    for (auto i = p->queue.begin ();
                                         i != p->queue.end (); i++)
                                        {
                                            bool cont = false;
                                            for (const auto &j : vc.second)
                                                {
                                                    if (j.second.user_id
                                                        == i->user_id)
                                                        {
                                                            cont = true;
                                                            break;
                                                        }
                                                }
                                            if (cont)
                                                continue;
                                            if (vector_find (&l_user,
                                                             i->user_id)
                                                == l_user.end ())
                                                l_user.push_back (i->user_id);
                                        }
                                }
                                for (const auto &i : l_user)
                                    rmed += p->remove_track_by_user (i);
                            }
                        catch (...)
                            {
                                event.reply ("`[ERROR]` Can't get current "
                                             "voice connection");
                                break;
                            }
                        size_t usiz = l_user.size ();
                        if (!usiz)
                            {
                                event.reply ("No user left this session yet");
                                break;
                            }
                        player_manager->update_info_embed (
                            event.command.guild_id);
                        event.reply (std::to_string (usiz) + " user"
                                     + std::string (usiz > 1 ? "s" : "")
                                     + " left this session. Removed "
                                     + std::to_string (rmed) + " track"
                                     + std::string (rmed > 1 ? "s" : ""));

                        break;
                    }
                case queue_modify_t::m_clear_musicat:
                    {
                        auto p = player_manager->get_player (
                            event.command.guild_id);
                        p->reset_shifted ();

                        const size_t rmed = p->remove_track_by_user (shaid);

                        event.reply (std::string ("Removed ")
                                     + std::to_string (rmed) + " track"
                                     + std::string (rmed > 1 ? "s" : "")
                                     + " from queue");

                        break;
                    }
                default:
                    event.reply ("Option not yet supported and is still in "
                                 "development");
                }
            return;
        }

    paginate::reply_paginated_playlist (event, queue);
}
}
}
}
