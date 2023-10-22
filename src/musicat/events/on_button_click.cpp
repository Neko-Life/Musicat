#include "musicat/events/on_button_click.h"
#include "musicat/cmds/progress.h"
#include "musicat/cmds/search.h"
#include "musicat/musicat.h"
#include "musicat/pagination.h"

namespace musicat::events
{
void
on_button_click (dpp::cluster *client)
{
    client->on_button_click ([] (const dpp::button_click_t &event) {
        const size_t fsub = event.custom_id.find ("/");
        const std::string cmd = event.custom_id.substr (0, fsub);

        if (cmd.empty ())
            return;

        if (cmd == "page_queue")
            {
                const std::string param = event.custom_id.substr (fsub + 1, 1);

                if (param.empty ())
                    {
                        fprintf (
                            stderr,
                            "[WARN] command \"page_queue\" have no param\n");

                        return;
                    }

                paginate::update_page (event.command.msg.id, param, event);
            }
        else if (cmd == "modal_p")
            {
                const std::string param
                    = event.custom_id.substr (fsub + 1, std::string::npos);

                if (param.empty ())
                    {
                        fprintf (stderr,
                                 "[WARN] command \"modal_p\" have no param\n");
                        return;
                    }

                if (param.find ("que_s_track") != std::string::npos)
                    {
                        event.dialog (
                            param.find ("top") != std::string::npos ? command::
                                    search::modal_enqueue_searched_track_top ()
                            : param.find ("slip") != std::string::npos
                                ? command::search::
                                    modal_enqueue_searched_track_slip ()
                                : command::search::
                                    modal_enqueue_searched_track (),
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
                        fprintf (
                            stderr,
                            "[WARN] modal_p param isn't handled: \"%s\"\n",
                            param.c_str ());
                    }
            }
        else if (cmd == "progress")
            {
                const std::string param
                    = event.custom_id.substr (fsub + 1, std::string::npos);

                if (param.empty ())
                    {
                        fprintf (
                            stderr,
                            "[WARN] command \"progress\" have no param\n");
                        return;
                    }

                if (param.find ("u") != std::string::npos)
                    {
                        command::progress::update_progress (event);
                    }
                else
                    {
                        fprintf (
                            stderr,
                            "[WARN] progress param isn't handled: \"%s\"\n",
                            param.c_str ());
                    }
            }
        else if (cmd == "playnow")
            {
                const std::string param
                    = event.custom_id.substr (fsub + 1, std::string::npos);

                if (param.empty ())
                    {
                        fprintf (stderr,
                                 "[WARN] command \"playnow\" have no param\n");
                        return;
                    }

                if (param.find ("u") != std::string::npos)
                    {
                        try
                            {
                                // if (debug) fprintf (stderr, "[playnow u cmd
                                // id, token: %ld %s\n]", event.command.id,
                                // event.command.token.c_str ());

                                dpp::interaction_create_t new_event (
                                    event.from, event.raw_event);

                                new_event.command = event.command;

                                auto player_manager
                                    = get_player_manager_ptr ();

                                // if (debug) fprintf (stderr, "[playnow u
                                // new_cmd id, token: %ld %s\n]",
                                // new_event.command.id,
                                // new_event.command.token.c_str ());

                                if (player_manager)
                                    player_manager->update_info_embed (
                                        event.command.guild_id, false,
                                        &new_event);
                            }
                        catch (const exception &e)
                            {
                                event.reply (
                                    std::string ("<@")
                                    + std::to_string (event.command.usr.id)
                                    + ">: " + e.what ());
                            }
                    }
                else
                    {
                        fprintf (
                            stderr,
                            "[WARN] playnow param isn't handled: \"%s\"\n",
                            param.c_str ());
                    }
            }
    });
}
} // musicat::events
