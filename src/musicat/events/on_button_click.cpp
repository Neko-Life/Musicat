#include "musicat/events/on_button_click.h"
#include "musicat/cmds.h"
#include "musicat/cmds/progress.h"
#include "musicat/cmds/search.h"
#include "musicat/musicat.h"
#include "musicat/pagination.h"
#include "musicat/util.h"

namespace musicat::events
{
using generic_handler_vec
    = std::pair<const char *, void (*) (const dpp::button_click_t &)>;

int
handle_generic_cmd (const dpp::button_click_t &event,
                    const std::string &command_name,
                    const generic_handler_vec *handlers)
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
page_queue (const dpp::button_click_t &event,
            const command::button_command_t &cmd)
{
    paginate::update_page (event.command.msg.id, cmd.param, event);
}

void
modal_p (const dpp::button_click_t &event,
         const command::button_command_t &cmd)
{
    const std::string param = cmd.param;

    if (param.empty ())
        return;

    if (param.find ("que_s_track") != std::string::npos)
        {
            event.dialog (
                param.find ("top") != std::string::npos
                    ? command::search::modal_enqueue_searched_track_top ()
                : param.find ("slip") != std::string::npos
                    ? command::search::modal_enqueue_searched_track_slip ()
                    : command::search::modal_enqueue_searched_track (),
                [] (const dpp::confirmation_callback_t &res) {
                    if (res.is_error ())
                        {
                            fprintf (stderr, "%s\n",
                                     res.http_info.body.c_str ());
                        }
                });
        }
    else
        {
            fprintf (stderr, "[WARN] modal_p param isn't handled: \"%s\"\n",
                     param.c_str ());
        }
}

inline constexpr const generic_handler_vec progress_commands[]
    = { { "u", command::progress::update_progress }, { NULL, NULL } };

void
progress (const dpp::button_click_t &event,
          const command::button_command_t &cmd)
{
    const std::string param = cmd.param;

    if (param.empty ())
        return;

    if (handle_generic_cmd (event, param, progress_commands) != 0)
        {
            fprintf (stderr, "[WARN] progress param isn't handled: \"%s\"\n",
                     param.c_str ());
        }
}

void
u_playnow (const dpp::button_click_t &event)
{
    auto player_manager = get_player_manager_ptr ();
    if (!player_manager)
        return;

    try
        {
            player_manager->update_info_embed (event.command.guild_id, false,
                                               &event);
        }
    catch (const exception &e)
        {
            event.reply (std::string ("<@")
                         + std::to_string (event.command.usr.id)
                         + ">: " + e.what ());
        }
}

void
p_playnow (const dpp::button_click_t &event)
{
    auto player_manager = get_player_manager_ptr ();
    if (!player_manager)
        return;

    try
        {
            dpp::voiceconn *v = event.from->get_voice (event.command.guild_id);
            auto vcuser = get_voice_from_gid (event.command.guild_id,
                                              event.command.usr.id);

            if (vcuser.first && v && v->voiceclient
                && !v->voiceclient->is_paused ()
                && v->channel_id == vcuser.first->id)
                {
                    player_manager->pause (event.from, event.command.guild_id,
                                           event.command.usr.id, false);
                }

            player_manager->update_info_embed (event.command.guild_id, false,
                                               &event);
        }
    catch (const exception &e)
        {
            event.reply (std::string ("<@")
                         + std::to_string (event.command.usr.id)
                         + ">: " + e.what ());
        }
}

void
r_playnow (const dpp::button_click_t &event)
{
    auto player_manager = get_player_manager_ptr ();
    if (!player_manager)
        return;

    try
        {
            dpp::voiceconn *v = event.from->get_voice (event.command.guild_id);
            auto vcuser = get_voice_from_gid (event.command.guild_id,
                                              event.command.usr.id);

            bool was_stopped = false;

            if (vcuser.first && v && v->voiceclient
                && v->voiceclient->is_paused ()
                && v->channel_id == vcuser.first->id)
                {
                    auto p
                        = player_manager->get_player (event.command.guild_id);

                    if (p)
                        was_stopped = p->is_stopped ();

                    player_manager->unpause (v->voiceclient,
                                             event.command.guild_id, false);
                }

            player_manager->update_info_embed (event.command.guild_id,
                                               was_stopped, &event);
        }
    catch (const exception &e)
        {
            event.reply (std::string ("<@")
                         + std::to_string (event.command.usr.id)
                         + ">: " + e.what ());
        }
}

void
s_playnow (const dpp::button_click_t &event)
{
    auto player_manager = get_player_manager_ptr ();
    if (!player_manager)
        return;

    try
        {
            dpp::voiceconn *v = event.from->get_voice (event.command.guild_id);
            auto vcuser = get_voice_from_gid (event.command.guild_id,
                                              event.command.usr.id);

            auto p = player_manager->create_player (event.command.guild_id);

            if (vcuser.first && v && v->channel_id == vcuser.first->id
                && player_manager->voice_ready (
                    event.command.guild_id, event.from, event.command.usr.id)
                && !util::is_player_not_playing (p, v))
                {
                    player_manager->stop_stream (event.command.guild_id);

                    p->skip (v);
                    p->set_stopped (true);
                    v->voiceclient->pause_audio (true);

                    player_manager->set_manually_paused (
                        event.command.guild_id);
                }

            player_manager->update_info_embed (event.command.guild_id, false,
                                               &event);
        }
    catch (const exception &e)
        {
            event.reply (std::string ("<@")
                         + std::to_string (event.command.usr.id)
                         + ">: " + e.what ());
        }
}

void
h_playnow (const dpp::button_click_t &event)
{
    auto player_manager = get_player_manager_ptr ();
    if (!player_manager)
        return;

    try
        {
            dpp::voiceconn *v = event.from->get_voice (event.command.guild_id);
            auto vcuser = get_voice_from_gid (event.command.guild_id,
                                              event.command.usr.id);

            if (vcuser.first && v && v->channel_id == vcuser.first->id)
                {
                    player_manager->shuffle_queue (event.command.guild_id,
                                                   false);
                }

            player_manager->update_info_embed (event.command.guild_id, false,
                                               &event);
        }
    catch (const exception &e)
        {
            event.reply (std::string ("<@")
                         + std::to_string (event.command.usr.id)
                         + ">: " + e.what ());
        }
}

void
e_playnow (const dpp::button_click_t &event)
{
    auto player_manager = get_player_manager_ptr ();
    if (!player_manager)
        return;

    try
        {
            player_manager->reply_info_embed (event, true, true);
        }
    catch (const exception &e)
        {
            event.reply (std::string ("<@")
                         + std::to_string (event.command.usr.id)
                         + ">: " + e.what ());
        }
}

void
x_playnow (const dpp::button_click_t &event)
{
    auto player_manager = get_player_manager_ptr ();
    if (!player_manager)
        return;

    try
        {
            player_manager->reply_info_embed (event, false, true);
        }
    catch (const exception &e)
        {
            event.reply (std::string ("<@")
                         + std::to_string (event.command.usr.id)
                         + ">: " + e.what ());
        }
}

void
d_playnow (const dpp::button_click_t &event)
{
    auto player_manager = get_player_manager_ptr ();
    if (!player_manager)
        return;

    try
        {
            auto p = player_manager->create_player (event.command.guild_id);
            p->notification = false;

            player_manager->update_info_embed (event.command.guild_id, false,
                                               &event);
        }
    catch (const exception &e)
        {
            event.reply (std::string ("<@")
                         + std::to_string (event.command.usr.id)
                         + ">: " + e.what ());
        }
}

void
b_playnow (const dpp::button_click_t &event)
{
    auto player_manager = get_player_manager_ptr ();
    if (!player_manager)
        return;

    try
        {
            auto p = player_manager->create_player (event.command.guild_id);
            p->notification = true;

            player_manager->update_info_embed (event.command.guild_id, false,
                                               &event);
        }
    catch (const exception &e)
        {
            event.reply (std::string ("<@")
                         + std::to_string (event.command.usr.id)
                         + ">: " + e.what ());
        }
}

inline constexpr const generic_handler_vec playnow_commands[]
    = { { "u", u_playnow }, { "p", p_playnow }, { "r", r_playnow },
        { "s", s_playnow }, { "h", h_playnow }, { "e", e_playnow },
        { "x", x_playnow }, { "d", d_playnow }, { "b", b_playnow },
        { NULL, NULL } };

void
playnow (const dpp::button_click_t &event,
         const command::button_command_t &cmd)
{
    const std::string param = cmd.param;

    if (param.empty ())
        return;

    if (handle_generic_cmd (event, param, playnow_commands) != 0)
        {
            fprintf (stderr, "[WARN] playnow param isn't handled: \"%s\"\n",
                     param.c_str ());
        }
}

void
message (const dpp::button_click_t &event,
         const command::button_command_t &cmd)
{
    const std::string param = cmd.param;

    if (param.empty ())
        return;

    if (param == "d")
        {
            event.from->creator->message_delete (event.command.msg.id,
                                                 event.command.msg.channel_id);
        }
    else
        {
            fprintf (stderr, "[WARN] message param isn't handled: \"%s\"\n",
                     param.c_str ());
        }
}

inline constexpr const command::button_handlers_map_t button_handlers
    = { { "page_queue", page_queue }, { "modal_p", modal_p },
        { "progress", progress },     { "playnow", playnow },
        { "message", message },       { NULL, NULL } };

void
on_button_click (dpp::cluster *client)
{
    client->on_button_click ([] (const dpp::button_click_t &event) {
        command::handle_button ({ button_handlers, event });
    });
}
} // musicat::events
