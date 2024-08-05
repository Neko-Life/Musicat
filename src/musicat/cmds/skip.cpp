#include "musicat/cmds/skip.h"
#include "musicat/cmds.h"
#include "musicat/cmds/seek.h"
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

int
run (dpp::discord_client *from, const dpp::snowflake &user_id,
     const dpp::snowflake &guild_id, std::string &out,
     int64_t param_amount = 1, int64_t param_remove = 0)
{
    auto pm_res = cmd_pre_get_player_manager_ready_werr (guild_id);
    if (pm_res.second == 1)
        {
            out = "Please wait while I'm getting ready to stream";
            return 1;
        }

    auto player_manager = pm_res.first;

    if (player_manager == NULL)
        return -1;

    try
        {
            auto v = from->get_voice (guild_id);
            int64_t am = param_amount;
            int64_t rm = param_remove;

            auto res = player_manager->skip (v, guild_id, user_id, am,
                                             rm ? true : false);
            switch (res.second)
                {
                case 0:
                    out = util::response::reply_skipped_track (res.first);
                    break;
                case -1:
                    out = "I'm not playing anything";
                    break;
                default:
                    out = std::to_string (res.second)
                          + util::join (res.second != 1, " member", "s")
                          + " voted to skip, add more vote to skip "
                            "current track";
                }

            return 1;
        }

    catch (const exception &e)
        {
            out = e.what ();
            return 1;
        }
}

void
slash_run (const dpp::slashcommand_t &event)
{
    int64_t am = 1;
    int64_t rm = 0;
    get_inter_param (event, "amount", &am);
    get_inter_param (event, "remove", &rm);

    std::string out;
    int status = run (event.from, event.command.usr.id, event.command.guild_id,
                      out, am, rm);
    if (status == 1)
        {
            event.reply (out);
        }
}

void
button_run_prev (const dpp::button_click_t &event)
{
    int status = seek::button_seek_zero (event);
    if (status == 0 || status != 128)
        return;

    auto guild_id = event.command.guild_id;

    auto player_manager = get_player_manager_ptr ();
    auto guild_player = player_manager->get_player (guild_id);

    std::lock_guard lk (guild_player->t_mutex);

    guild_player->reset_shifted ();

    if (!guild_player->current_track.is_empty ())
        guild_player->current_track.repeat = 0;

    if (!guild_player->queue.empty ())
        {
            dpp::voiceconn *v = event.from->get_voice (guild_id);

            bool stopping = false;
            // stop it! it will be resumed by auto marker at the ends of every
            // stream
            if (v && v->voiceclient
                && v->voiceclient->get_secs_remaining () > 0.05f)
                stopping = player_manager->stop_stream (guild_id) == 0;

            guild_player->queue.front ().repeat = 0;

            // move the back of the queue to front
            guild_player->queue.push_front (guild_player->queue.back ());
            guild_player->queue.pop_back ();

            // we havent had any playback session?
            if (!stopping)
                v->voiceclient->insert_marker ("e");
        }
    else
        fprintf (
            stderr,
            "[command::skip::button_run_prev WARN] Track queue is empty: %s\n",
            event.command.guild_id.str ().c_str ());

    player_manager->update_info_embed (event.command.guild_id, false, &event);
}

void
button_run_next (const dpp::button_click_t &event)
{
    auto player_manager = get_player_manager_ptr ();
    if (player_manager == NULL)
        return;

    std::string out;
    run (event.from, event.command.usr.id, event.command.guild_id, out);

    player_manager->update_info_embed (event.command.guild_id, false, &event);
}

} // musicat::command::skip

// vim: et sw=4 ts=8
