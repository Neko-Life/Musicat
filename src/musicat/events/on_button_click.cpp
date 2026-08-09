// clang-format off
#include "musicat/pagination.h"
#include "musicat/player_manager.h"
// clang-format on

#include "musicat/events/on_button_click.h"
#include "musicat/cmds.h"
#include "musicat/cmds/loop.h"
#include "musicat/cmds/pause.h"
#include "musicat/cmds/play.h"
#include "musicat/cmds/progress.h"
#include "musicat/cmds/search.h"
#include "musicat/cmds/seek.h"
#include "musicat/cmds/skip.h"
#include "musicat/musicat.h"
#include "musicat/server/ws/player.h"
#include "musicat/task.h"
#include "musicat/util_response.h"

namespace musicat::events
{
using generic_handler_vec = std::pair<const char *, void (*) (const dpp::button_click_t &)>;

int
handle_generic_cmd (const dpp::button_click_t &event, const std::string &command_name, const generic_handler_vec *handlers)
{
    if (!handlers)
        return -1;

    void (*handler) (const dpp::button_click_t &) = NULL;

    for (size_t i = 0; handlers[i].first != NULL; i++)
        {
            if (handlers[i].first != command_name)
                {
                    continue;
                }

            handler = handlers[i].second;
            break;
        }

    if (!handler)
        return 1;

    handler (event);

    return 0;
}

void
page_queue (const dpp::button_click_t &event, const command::button_command_t &cmd)
{
    paginate::update_page (event.command.msg.id, cmd.param, event);
}

void
modal_p (const dpp::button_click_t &event, const command::button_command_t &cmd)
{
    const std::string param = cmd.param;

    if (param.empty ())
        return;

    if (param.find ("que_s_track") != std::string::npos)
        {
            event.dialog (param.find ("top") != std::string::npos    ? command::search::modal_enqueue_searched_track_top ()
                          : param.find ("slip") != std::string::npos ? command::search::modal_enqueue_searched_track_slip ()
                                                                     : command::search::modal_enqueue_searched_track (),
                          [] (const dpp::confirmation_callback_t &res)
                              {
                                  if (res.is_error ())
                                      {
                                          fprintf (stderr, "%s\n", res.http_info.body.c_str ());
                                      }
                              });
        }
    else
        {
            fprintf (stderr, "[WARN] modal_p param isn't handled: \"%s\"\n", param.c_str ());
        }
}

inline constexpr const generic_handler_vec progress_commands[] = { { "u", command::progress::update_progress }, { NULL, NULL } };

void
progress (const dpp::button_click_t &event, const command::button_command_t &cmd)
{
    const std::string param = cmd.param;

    if (param.empty ())
        return;

    if (handle_generic_cmd (event, param, progress_commands) != 0)
        {
            fprintf (stderr, "[WARN] progress param isn't handled: \"%s\"\n", param.c_str ());
        }
}

void
u_playnow (const dpp::button_click_t &event)
{
    int ret = player::manager::update_info_embed (event.command.guild_id, false, &event);
    if (ret > 0)
        event.reply (util::response::str_mention_user (event.command.usr.id)
                     + (ret == 1 ? "Nothing is playing right now" : "No permission to send/update message"));
}

void
p_playnow (const dpp::button_click_t &event)
{
    command::pause::button_run (event);
}

void
r_playnow (const dpp::button_click_t &event)
{
    command::play::button_run (event);
}

void
s_playnow (const dpp::button_click_t &event)
{
    dpp::voiceconn *v = event.from ()->get_voice (event.command.guild_id);
    auto vcuser = get_voice_from_gid (event.command.guild_id, event.command.usr.id);

    auto p = player::manager::create_player (event.command.guild_id);

    if (p && vcuser.first && v && v->voiceclient && v->channel_id == vcuser.first->id
        && player::manager::voice_ready (event.command.guild_id, event.from ()->shard_id, event.command.usr.id) && !p->stopped)
        {
            p->skip_playback (v);
            p->stopped = true;
            v->voiceclient->pause_audio (true);

            player::manager::set_manually_paused (event.command.guild_id);
        }

    int ret = player::manager::update_info_embed (event.command.guild_id, false, &event);
    if (ret > 0)
        event.reply (util::response::str_mention_user (event.command.usr.id)
                     + (ret == 1 ? "Nothing is playing right now" : "No permission to send/update message"));
}

void
h_playnow (const dpp::button_click_t &event)
{
    dpp::voiceconn *v = event.from ()->get_voice (event.command.guild_id);
    auto vcuser = get_voice_from_gid (event.command.guild_id, event.command.usr.id);

    if (vcuser.first && v && v->channel_id == vcuser.first->id && player::manager::shuffle_queue (event.command.guild_id, false))
        {
            server::ws::player::publish_queue (event.command.guild_id);

            int ret = player::manager::update_info_embed (event.command.guild_id, false, &event);
            if (ret > 0)
                event.reply (util::response::str_mention_user (event.command.usr.id)
                             + (ret == 1 ? "Nothing is playing right now" : "No permission to send/update message"));
        }
}

void
e_playnow (const dpp::button_click_t &event)
{
    try
        {
            player::manager::reply_info_embed (event, true, true);
        }
    catch (const exception &e)
        {
            event.reply (util::response::str_mention_user (event.command.usr.id) + e.what ());
        }
}

void
x_playnow (const dpp::button_click_t &event)
{
    try
        {
            player::manager::reply_info_embed (event, false, true);
        }
    catch (const exception &e)
        {
            event.reply (util::response::str_mention_user (event.command.usr.id) + e.what ());
        }
}

void
d_playnow (const dpp::button_click_t &event)
{
    auto p = player::manager::create_player (event.command.guild_id);
    if (p)
        p->notification = false;

    int ret = player::manager::update_info_embed (event.command.guild_id, false, &event);
    if (ret > 0)
        event.reply (util::response::str_mention_user (event.command.usr.id)
                     + (ret == 1 ? "Nothing is playing right now" : "No permission to send/update message"));
}

void
b_playnow (const dpp::button_click_t &event)
{
    auto p = player::manager::create_player (event.command.guild_id);
    if (p)
        p->notification = true;

    int ret = player::manager::update_info_embed (event.command.guild_id, false, &event);
    if (ret > 0)
        event.reply (util::response::str_mention_user (event.command.usr.id)
                     + (ret == 1 ? "Nothing is playing right now" : "No permission to send/update message"));
}

void
l_playnow (const dpp::button_click_t &event)
{
    try
        {
            command::loop::handle_button_modal_dialog (event);
        }
    catch (const exception &e)
        {
            event.reply (util::response::str_mention_user (event.command.usr.id) + e.what ());
        }
}

void
a_playnow (const dpp::button_click_t &event)
{
    task::run (
        [event] ()
            {
                auto guild_player = player::manager::create_player (event.command.guild_id);
                if (guild_player)
                    guild_player->set_auto_play (!guild_player->auto_play);

                int ret = player::manager::update_info_embed (event.command.guild_id, false, &event);
                if (ret > 0)
                    event.reply (util::response::str_mention_user (event.command.usr.id)
                                 + (ret == 1 ? "Nothing is playing right now" : "No permission to send/update message"));
            });
}

void
w_playnow (const dpp::button_click_t &event)
{
    command::seek::button_run_rewind (event);
}

void
f_playnow (const dpp::button_click_t &event)
{
    command::seek::button_run_forward (event);
}

void
v_playnow (const dpp::button_click_t &event)
{
    command::skip::button_run_prev (event);
}

void
n_playnow (const dpp::button_click_t &event)
{
    command::skip::button_run_next (event);
}

inline constexpr const generic_handler_vec playnow_commands[]
    = { { "u", u_playnow }, { "p", p_playnow }, { "r", r_playnow }, { "s", s_playnow }, { "h", h_playnow }, { "e", e_playnow },
        { "x", x_playnow }, { "d", d_playnow }, { "b", b_playnow }, { "l", l_playnow }, { "a", a_playnow }, { "w", w_playnow },
        { "f", f_playnow }, { "v", v_playnow }, { "n", n_playnow }, { NULL, NULL } };

void
playnow (const dpp::button_click_t &event, const command::button_command_t &cmd)
{
    const std::string param = cmd.param;

    if (param.empty ())
        return;

    if (handle_generic_cmd (event, param, playnow_commands) != 0)
        {
            fprintf (stderr, "[WARN] playnow param isn't handled: \"%s\"\n", param.c_str ());
        }
}

void
message (const dpp::button_click_t &event, const command::button_command_t &cmd)
{
    const std::string param = cmd.param;

    if (param.empty ())
        return;

    if (param == "d")
        {
            event.from ()->creator->message_delete (event.command.msg.id, event.command.msg.channel_id);
        }
    else
        {
            fprintf (stderr, "[WARN] message param isn't handled: \"%s\"\n", param.c_str ());
        }
}

inline constexpr const command::button_handlers_map_t button_handlers
    = { { "page_queue", page_queue }, { "modal_p", modal_p }, { "progress", progress },
        { "playnow", playnow },       { "message", message }, { NULL, NULL } };

void
on_button_click (dpp::cluster *client)
{
    client->on_button_click ([] (const dpp::button_click_t &event) { command::handle_button ({ button_handlers, event }); });
}

} //  namespace musicat::events
