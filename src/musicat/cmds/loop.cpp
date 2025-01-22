#include "musicat/cmds/loop.h"
#include "musicat/cmds.h"
#include "musicat/mctrack.h"
#include "musicat/musicat.h"
#include "musicat/thread_manager.h"
#include "musicat/util.h"
#include "musicat/util_response.h"

#define MIN_VAL 0
// just let discord set the max val
// #define MAX_VAL 10000000

namespace musicat::command::loop
{
dpp::slashcommand
get_register_obj (const dpp::snowflake &sha_id)
{
    return dpp::slashcommand ("loop", "Configure [repeat] mode", sha_id)
        .add_option (dpp::command_option (dpp::co_integer, "mode",
                                          "Set [to this] mode", true)
                         .add_choice (dpp::command_option_choice (
                             "One", player::loop_mode_t::l_song))
                         .add_choice (dpp::command_option_choice (
                             "Queue", player::loop_mode_t::l_queue))
                         .add_choice (dpp::command_option_choice (
                             "One/Queue", player::loop_mode_t::l_song_queue))
                         .add_choice (dpp::command_option_choice (
                             "Off", player::loop_mode_t::l_none)))
        .add_option (
            dpp::command_option (dpp::co_integer, "loop-amount",
                                 "Only[ valid] for `One`, "
                                 "loop this track only for [number] of times")
                .set_min_value (MIN_VAL)
            /*.set_max_value (MAX_VAL)*/);
}

template <typename T>
auto
get_mode_param_getter (const T &event)
{
    return [event] (int64_t &a_l, int64_t &) {
        get_inter_param (event, "mode", &a_l);
    };
}

template <typename T>
auto
get_loop_amount_param_getter (const T &event)
{
    return [event] (int64_t &l_amount) {
        get_inter_param (event, "loop-amount", &l_amount);
    };
}

int
run (const dpp::snowflake &guild_id, const dpp::snowflake &user_id,
     dpp::discord_client *from,
     std::function<void (int64_t &, int64_t &)> get_mode_param,
     std::function<void (int64_t &)> get_loop_amount_param,
     std::string &out_reply, bool update_info_embed = true)
{
    auto pm_res = cmd_pre_get_player_manager_ready_werr (guild_id);
    if (pm_res.second == 1)
        {
            out_reply = "Please wait while I'm getting ready to stream";
            return 1;
        }

    auto player_manager = pm_res.first;

    if (player_manager == NULL)
        return -1;

    const dpp::snowflake sha_id = get_sha_id ();

    std::pair<dpp::channel *, std::map<dpp::snowflake, dpp::voicestate> > uvc;
    std::pair<dpp::channel *, std::map<dpp::snowflake, dpp::voicestate> > cvc;

    // !TODO: add dj role to bypass all these restriction
    // also move these checks to a function for more DRY
    uvc = get_voice_from_gid (guild_id, user_id);
    if (!uvc.first)
        {
            out_reply = "You're not in a voice channel";
            return 1;
        }

    cvc = get_voice_from_gid (guild_id, sha_id);
    if (!cvc.first)
        {
            out_reply = "I'm not playing anything right now";
            return 1;
        }

    if (uvc.first->id != cvc.first->id)
        {
            out_reply = "You're not in my voice channel";
            return 1;
        }

    auto guild_player = player_manager->create_player (guild_id);
    guild_player->from = from;

    if (guild_player->saved_config_loaded != true)
        player_manager->load_guild_player_config (guild_id);

    int64_t a_l = 0, current_val = (int64_t)guild_player->loop_mode;
    get_mode_param (a_l, current_val);

    if (a_l < 0 || a_l > 3)
        {
            out_reply = "Invalid mode";
            return 1;
        }

    int64_t l_amount = -1;
    get_loop_amount_param (l_amount);

    // make sure nothing messing with track order
    guild_player->reset_shifted ();

    const bool set_loop_amount = !guild_player->current_track.is_empty ()
                                 && l_amount >= MIN_VAL
                                 && (a_l == player::loop_mode_t::l_song);

    int status = 0;

    if (set_loop_amount)
        {
            // repeat current song only amount times

            // // set to maximum value if its over max
            // if (l_amount > MAX_VAL)
            //     l_amount = MAX_VAL;

            guild_player->current_track.repeat = l_amount;
            if (guild_player->queue.size () > 0)
                guild_player->queue.front ().repeat = l_amount;
            else
                guild_player->queue.push_back (guild_player->current_track);

            const bool set_to_no_repeat = l_amount == 0;

            const std::string pre
                = std::string (set_to_no_repeat ? "Won't" : "Will");

            const std::string suf
                = (set_to_no_repeat
                       ? "anymore"
                       : "for " + std::to_string (l_amount)
                             + util::join (l_amount != 1, " more time", "s"));

            const std::string rep
                = pre + " repeat `"
                  + mctrack::get_title (guild_player->current_track) + "` "
                  + suf;

            out_reply = rep;
            status = 1;
        }
    else // actually set loop mode
        {
            if (guild_player->loop_mode == a_l)
                {
                    out_reply = "Already set to that mode";
                    return 1;
                }

            guild_player->set_loop_mode (a_l);

            constexpr const char *loop_message[]
                = { "Turned off repeat mode", "Set to repeat a song",
                    "Set to repeat queue",
                    "Set to repeat a song and not to remove skipped song" };

            out_reply = loop_message[a_l];
            status = 1;
        }

    if (update_info_embed)
        try
            {
                player_manager->update_info_embed (guild_id);
            }
        catch (...)
            {
                // Meh
            }

    return status;
}

void
slash_run (const dpp::slashcommand_t &event)
{
    std::thread t ([event] () {
        thread_manager::DoneSetter tmds;

        std::string out_reply;
        int status = run (event.command.guild_id, event.command.usr.id,
                          event.from(), get_mode_param_getter (event),
                          get_loop_amount_param_getter (event), out_reply);

        if (status == 1 && !out_reply.empty ())
            {
                event.reply (out_reply);
            }
    });

    thread_manager::dispatch (t);
}

void
button_modal_dialog (const dpp::button_click_t &event)
{
    // loop modal dialog
    dpp::component mode_inp;
    mode_inp.set_type (dpp::cot_selectmenu)
        .set_label ("Select loop mode:")
        .set_id ("mode")
        // turns out modal only supports text type, so my idea to use select
        // menu on a modal is failed
        //
        // sadge
        .add_select_option (dpp::select_option ("One", "1", "Loop track"))
        .add_select_option (dpp::select_option ("Queue", "2", "Loop queue"))
        .add_select_option (dpp::select_option (
            "One/Queue", "3", "Loop track and don't remove skipped track"))
        .add_select_option (dpp::select_option ("Off", "0", "Disable loop"));

    dpp::component amount_inp = util::response::create_short_text_input (
        "Loop amount (only for One):", "amount");

    dpp::interaction_modal_response modal ("loop_mode", "Loop mode");
    modal.add_component (dpp::component ().add_component (mode_inp))
        .add_component (dpp::component ().add_component (amount_inp));

    event.dialog (modal);
}

void
handle_button_modal_dialog (const dpp::button_click_t &event)
{
    std::thread t ([event] () {
        thread_manager::DoneSetter tmds;

        auto player_manager = get_player_manager_ptr ();
        if (!player_manager)
            return;

        std::string out_reply;
        // int status =

        run (
            event.command.guild_id, event.command.usr.id, event.from(),
            [event] (int64_t &a_l, int64_t &current_val) {
                if (current_val >= 3)
                    a_l = 0;
                else
                    a_l = current_val + 1;
            },
            [event] (int64_t &) {}, out_reply, false);

        player_manager->update_info_embed (event.command.guild_id, false,
                                           &event);
    });

    thread_manager::dispatch (t);
}

} // musicat::command::loop
