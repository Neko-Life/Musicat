#include "musicat/cmds/playlist.h"
#include "musicat/autocomplete.h"
#include "musicat/cmds.h"
#include "musicat/db.h"
#include "musicat/mctrack.h"
#include "musicat/musicat.h"
#include "musicat/pagination.h"
#include "musicat/thread_manager.h"
#include "musicat/util_response.h"
#include <libpq-fe.h>
#include <memory>
#include <variant>

template <typename T>
static void
_get_nested_arg (const dpp::interaction_create_t &event, const size_t at,
                 T *result)
{
    dpp::command_interaction cmd = event.command.get_command_interaction ();

    if (!cmd.options.size ())
        return;

    auto &command_options = cmd.options.at (0).options;

    if (command_options.size () <= at)
        return;

    dpp::command_value val = command_options.at (at).value;

    if (!std::holds_alternative<T> (val))
        return;

    *result = std::get<T> (val);
}

static std::string
_get_id_arg (const dpp::interaction_create_t &event)
{
    std::string ret = "";
    _get_nested_arg<std::string> (event, 0, &ret);
    return ret;
}

static int64_t
_get_top_arg (const dpp::interaction_create_t &event)
{
    int64_t ret = 0;
    _get_nested_arg<int64_t> (event, 1, &ret);
    return ret;
}

namespace musicat::command::playlist
{
namespace autocomplete
{
void
id (const dpp::autocomplete_t &event, const std::string &param)
{
    std::pair<PGresult *, ExecStatusType> res
        = database::get_all_user_playlist (event.command.usr.id,
                                           database::gup_name_only);

    std::vector<std::pair<std::string, std::string> > response = {};

    if (res.second == PGRES_TUPLES_OK)
        {
            int rn = 0;
            while (true)
                {
                    if (PQgetisnull (res.first, rn, 0))
                        break;

                    std::string val
                        = std::string (PQgetvalue (res.first, rn, 0));

                    response.push_back (std::make_pair (val, val));
                    rn++;
                }
        }

    database::finish_res (res.first);
    res.first = nullptr;

    musicat::autocomplete::create_response (
        musicat::autocomplete::filter_candidates (response, param), event);
}
} // autocomplete

namespace save
{
dpp::command_option
get_option_obj ()
{
    return dpp::command_option (dpp::co_sub_command, "save",
                                "Save [current playlist]")
        .add_option (dpp::command_option (
                         dpp::co_string, "id",
                         "Playlist name", //", must match validation regex "
                         // "/^[0-9a-zA-Z_-]{1,100}$/",
                         true)
                         .set_auto_complete (true));
}

void
slash_run (const dpp::slashcommand_t &event)
{
    auto player_manager = get_player_manager_ptr ();
    if (!player_manager)
        {
            return;
        }

    std::deque<player::MCTrack> q
        = player_manager->get_queue (event.command.guild_id);
    size_t q_size = q.size ();
    if (!q_size)
        {
            event.reply ("Not a single track currently waiting in the queue");
            return;
        }

    const std::string p_id = _get_id_arg (event);

    // if (!database::valid_name (p_id))
    //     {
    //         event.reply ("Invalid `id` format!");
    //         return;
    //     }

    event.thinking ();

    ExecStatusType res
        = database::update_user_playlist (event.command.usr.id, p_id, q);

    event.edit_response (res == PGRES_COMMAND_OK
                             ? std::string ("Saved playlist containing ")
                                   + std::to_string (q_size) + " track"
                                   + (q_size > 1 ? "s" : "")
                                   + " with Id: " + p_id
                             : "Somethin went wrong, can't save playlist");
}
} // save

namespace load
{
dpp::command_option
get_option_obj ()
{
    return dpp::command_option (dpp::co_sub_command, "load",
                                "Load [saved playlist]")
        .add_option (dpp::command_option (dpp::co_string, "id",
                                          "Playlist name [to load]", true)
                         .set_auto_complete (true))
        .add_option (create_yes_no_option (
            "top", "Add [these song] to the top [of the queue]"));
}

void
slash_run (const dpp::slashcommand_t &event)
{
    std::thread rt ([event] () {
        thread_manager::DoneSetter tmds;

        auto player_manager = get_player_manager_ptr ();
        if (!player_manager)
            {
                return;
            }

        const std::string p_id = _get_id_arg (event);
        int64_t arg_top = _get_top_arg (event);

        event.thinking ();

        const bool view
            = event.command.get_command_interaction ().options.at (0).name
              == "view";

        std::pair<PGresult *, ExecStatusType> res
            = database::get_user_playlist (event.command.usr.id, p_id,
                                           database::gup_raw_only);

        std::pair<std::deque<player::MCTrack>, int> playlist_res
            = database::get_playlist_from_PGresult (res.first);

        int retnow = 0;

        if (playlist_res.second == -2)
            {
                if (!res.first)
                    {
                        if (res.second == (ExecStatusType)-3)
                            event.edit_response ("Invalid `id` format!");
                        else
                            {
                                event.edit_response (
                                    std::string ("`[ERROR]` Unexpected error "
                                                 "getting user "
                                                 "playlist with code: ")
                                    + std::to_string (res.second));
                                fprintf (
                                    stderr,
                                    "[CMD_PLAYLIST_ERROR] Unexpected error "
                                    "database::get_user_playlist with code: "
                                    "%d\n",
                                    res.second);
                            }
                    }
                else
                    event.edit_response ("Unknown playlist");

                retnow = 1;
            }
        else if (playlist_res.second == -1)
            {
                event.edit_response (
                    "This playlist is empty, save a new one with "
                    "the same Id to overwrite it");
                retnow = 1;
            }

        database::finish_res (res.first);
        res.first = nullptr;

        if (retnow)
            return;

        std::shared_ptr<player::Player> guild_player;
        size_t count = 0;

        std::deque<player::MCTrack> q = {};

        if (view != true)
            {
                guild_player
                    = player_manager->create_player (event.command.guild_id);
                guild_player->from = event.from;
            }

        const bool add_to_top = arg_top ? true : false;
        const bool debug = get_debug_state ();

        if (debug)
            fprintf (stderr, "[playlist::load] arg_top add_to_top: %ld %d\n",
                     arg_top, add_to_top);

        std::deque<player::MCTrack> to_iter = {};

        if (add_to_top)
            {
                for (auto &d : playlist_res.first)
                    {
                        if (debug)
                            fprintf (
                                stderr,
                                "[playlist::load] Pushed to front: '%s'\n",
                                mctrack::get_title (d).c_str ());

                        to_iter.push_front (d);
                    }
            }
        else
            {
                to_iter = playlist_res.first;
            }

        for (auto &t : to_iter)
            {
                t.user_id = event.command.usr.id;
                if (view)
                    q.push_back (t);
                else
                    {
                        guild_player->add_track (
                            t, add_to_top, event.command.guild_id, false);
                        count++;
                    }
            }

        if (view)
            paginate::reply_paginated_playlist (event, q, p_id, true);
        else
            {
                if (count)
                    try
                        {
                            player_manager->update_info_embed (
                                event.command.guild_id);
                        }
                    catch (...)
                        {
                        }

                event.edit_response (util::response::reply_added_playlist (
                    p_id, arg_top, count));

                // !TODO: this is probably for connect and play when adding
                // playlist but bot isn't in user vc
                //
                // std::pair<dpp::channel*, std::map<dpp::snowflake,
                // dpp::voicestate>> c; bool has_c = false; bool no_vc = false;
                // try
                // {
                //     c = get_voice_from_gid(event.command.guild_id,
                //     event.command.usr.id); has_c = true;
                // }
                // catch (...) {}

                // try
                // {
                //     get_voice_from_gid(event.command.guild_id,
                //     event.from->creator->me.id);
                // }
                // catch (...)
                // {
                //     no_vc = true;
                // }

                // if (has_c && no_vc && c.first &&
                // has_permissions_from_ids(event.command.guild_id,
                //                                                           event.from->creator->me.id,
                //                                                           c.first->id,
                //                                                           {
                //                                                           dpp::p_view_channel,dpp::p_connect
                //                                                           }))
                // {
                //     guild_player->set_channel(event.command.channel_id);

                //     {
                //         std::lock_guard<std::mutex> lk(player_manager->c_m);
                //         std::lock_guard<std::mutex>
                //         lk2(player_manager->wd_m);
                //         player_manager->connecting.insert_or_assign(event.command.guild_id,
                //         c.first->id);
                //         player_manager->waiting_vc_ready.insert_or_assign(event.command.guild_id,
                //         "2");
                //     }

                //     std::thread t([player_manager, event]() {
                //                       player_manager->reconnect(event.from,
                //                       event.command.guild_id);
                //                   });
                //     t.detach();
                // }
            }
    });

    thread_manager::dispatch (rt);
}
} // load

namespace view
{
dpp::command_option
get_option_obj ()
{
    return dpp::command_option (dpp::co_sub_command, "view",
                                "View [saved playlist] tracks")
        .add_option (dpp::command_option (dpp::co_string, "id",
                                          "Playlist name [to view]", true)
                         .set_auto_complete (true));
}

void
slash_run (const dpp::slashcommand_t &event)
{
    load::slash_run (event);
}
} // view

namespace delete_
{
dpp::command_option
get_option_obj ()
{
    return dpp::command_option (dpp::co_sub_command, "delete",
                                "Delete [saved playlist]")
        .add_option (dpp::command_option (dpp::co_string, "id",
                                          "Playlist name [to delete]", true)
                         .set_auto_complete (true));
}

void
slash_run (const dpp::slashcommand_t &event)
{
    const std::string p_id = _get_id_arg (event);
    event.thinking ();

    // if (!database::valid_name (p_id))
    //     {
    //         event.edit_response ("Invalid `id` format!");
    //         return;
    //     }

    if (database::delete_user_playlist (event.command.usr.id, p_id)
        == PGRES_TUPLES_OK)
        event.edit_response (std::string ("Deleted playlist ") + p_id);
    else
        event.edit_response ("Unknown playlist");
}
} // delete_

dpp::slashcommand
get_register_obj (const dpp::snowflake &sha_id)
{
    return dpp::slashcommand ("playlist", "Your playlist manager", sha_id)
        .add_option (save::get_option_obj ())
        .add_option (load::get_option_obj ())
        .add_option (view::get_option_obj ())
        .add_option (delete_::get_option_obj ());
}

void
slash_run (const dpp::slashcommand_t &event)
{
    auto inter = event.command.get_command_interaction ();

    if (inter.options.begin () == inter.options.end ())
        {
            fprintf (stderr, "[command::playlist::slash_run ERROR] !!! No "
                             "options for command 'playlist' !!!\n");

            event.reply ("I don't have that command, how'd you get that?? "
                         "Please report this to my developer");

            return;
        }

    const std::string cmd = inter.options.at (0).name;

    // !TODO: refactor: use command_handlers_map_t
    if (cmd == "save")
        save::slash_run (event);
    else if (cmd == "load")
        load::slash_run (event);
    else if (cmd == "view")
        view::slash_run (event);
    else if (cmd == "delete")
        delete_::slash_run (event);
    else
        {
            fprintf (
                stderr,
                "[ERROR] !!! NO SUB-COMMAND '%s' FOR COMMAND 'playlist' !!!\n",
                cmd.c_str ());
            event.reply ("Somethin went horribly wrong.... Please quickly "
                         "report this to my developer");
        }
} // slash_run
} // musicat::command::playlist

// vim: et ts=8 sw=4
