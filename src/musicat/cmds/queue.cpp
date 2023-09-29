#include "musicat/cmds.h"
#include "musicat/pagination.h"

namespace musicat
{
namespace command
{
namespace queue
{
// =================== PRIVATE ===================

void
handle_option (int64_t &qarg, const dpp::interaction_create_t &event,
               player::player_manager_ptr player_manager,
               std::deque<player::MCTrack> &queue,
               const dpp::snowflake &sha_id)
{
    auto guild_player = player_manager->get_player (event.command.guild_id);

    std::lock_guard<std::mutex> lk (guild_player->t_mutex);
    guild_player->reset_shifted ();

    switch (qarg)
        {
        case queue_modify_t::m_clear:
            {
                auto siz = queue.size ();
                if (siz < 2)
                    {
                        event.edit_response (
                            "No track to clear, skip the current "
                            "track to clear the queue completely");
                        break;
                    }

                {
                    player::MCTrack t = guild_player->queue.at (0);
                    guild_player->queue.clear ();
                    guild_player->queue.push_back (t);
                }

                player_manager->update_info_embed (event.command.guild_id);

                event.edit_response ("Cleared");

                break;
            }
        case queue_modify_t::m_shuffle:
            {
                if (player_manager->shuffle_queue (event.command.guild_id))
                    {
                        event.edit_response (
                            "Shuffled"
                            + std::string (queue.size () < 5
                                               ? ", I suggest the `/move` "
                                                 "command if you do think my "
                                                 "shuffling skill isn't very "
                                                 "much satisfying"
                                               : ""));
                        break;
                    }
                else
                    {
                        event.edit_response ("Doesn't seem like shuffleable");
                        break;
                    }
            }
        case queue_modify_t::m_reverse:
            {
                auto siz = queue.size ();
                if (siz < 3)
                    {
                        event.edit_response ("Reversed. Not that you can tell "
                                             "the difference");
                        break;
                    }
                {
                    player::MCTrack t = guild_player->queue.at (0);

                    std::deque<player::MCTrack> n_queue = {};
                    for (size_t i = (siz - 1); i > 0; i--)
                        n_queue.push_back (guild_player->queue.at (i));

                    guild_player->queue.clear ();
                    guild_player->queue = std::move (n_queue);
                    guild_player->queue.push_front (t);
                }
                player_manager->update_info_embed (event.command.guild_id);

                event.edit_response ("Reversed");

                break;
            }
        case queue_modify_t::m_clear_left:
            {
                // we need to be in the vc where we check which users left the
                // vc currently no check for user who invoked the command (it's
                // rather a feature where user who left the vc can clear their
                // tracks to keep their friend happy)
                if (!player_manager->voice_ready (event.command.guild_id,
                                                  event.from,
                                                  event.command.usr.id))
                    {
                        event.edit_response (
                            "Establishing connection. Please wait...");
                        break;
                    }

                // removed count
                size_t rmed = 0;
                // users who have added a track but not in the vc
                std::vector<dpp::snowflake> l_user = {};

                try
                    {
                        auto vc = get_voice_from_gid (event.command.guild_id,
                                                      sha_id);

                        // users in the vc that have been checked
                        std::vector<dpp::snowflake> all_user = {};
                        // check each track for missing owner
                        for (auto i = guild_player->queue.begin ();
                             i != guild_player->queue.end (); i++)
                            {
                                // whether the user of this track already
                                // checked
                                if (vector_find (&all_user, i->user_id)
                                    != all_user.end ())
                                    continue;

                                // save as checked user
                                all_user.push_back (i->user_id);

                                // whether this track have its owner in vc
                                bool cont = false;
                                for (const auto &j : vc.second)
                                    {
                                        if (j.second.user_id == i->user_id)
                                            {
                                                cont = true;
                                                break;
                                            }
                                    }
                                // skip if it is
                                if (cont)
                                    continue;

                                // add for delete candidate
                                l_user.push_back (i->user_id);
                            }

                        // because it's modifying the queue, it should be in
                        // different loop than above loop otherwise it will be
                        // catastrophic
                        for (const auto &i : l_user)
                            rmed += guild_player->remove_track_by_user (i);
                    }
                catch (...)
                    {
                        event.edit_response ("`[ERROR]` Can't get current "
                                             "voice connection");
                        break;
                    }
                size_t usiz = l_user.size ();
                if (!usiz)
                    {
                        event.edit_response ("No user left this session yet");
                        break;
                    }
                player_manager->update_info_embed (event.command.guild_id);

                event.edit_response (std::to_string (usiz) + " user"
                                     + std::string (usiz > 1 ? "s" : "")
                                     + " left this session. Removed "
                                     + std::to_string (rmed) + " track"
                                     + std::string (rmed > 1 ? "s" : ""));

                break;
            }
        case queue_modify_t::m_clear_musicat:
            {
                const size_t rmed
                    = guild_player->remove_track_by_user (sha_id);

                event.edit_response (
                    std::string ("Removed ") + std::to_string (rmed) + " track"
                    + std::string (rmed > 1 ? "s" : "") + " from queue");

                break;
            }
        default:
            event.edit_response ("Option not yet supported and is still in "
                                 "development");
        }
}

// =================== PRIVATE END ===================

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
slash_run (const dpp::slashcommand_t &event)
{
    auto player_manager = get_player_manager_ptr ();
    if (!player_manager)
        {
            return;
        }

    const dpp::snowflake sha_id = event.from->creator->me.id;

    // avoid getting timed out when loading large queue
    event.thinking ();
    player_manager->load_guild_current_queue (event.command.guild_id, &sha_id);

    std::deque<player::MCTrack> queue
        = player_manager->get_queue (event.command.guild_id);

    if (queue.empty ())
        {
            event.edit_response ("No track");
            return;
        }

    int64_t qarg = -1;
    get_inter_param (event, "action", &qarg);

    if (qarg > -1)
        return handle_option (qarg, event, player_manager, queue, sha_id);

    paginate::reply_paginated_playlist (event, queue, "Queue", true);
}
} // queue
} // command
} // musicat
