#include "musicat/cmds.h"
#include "musicat/db.h"
#include "musicat/musicat.h"
#include "musicat/pagination.h"
#include "musicat/player.h"
#include "musicat/runtime_cli.h"
#include "musicat/server.h"
#include "musicat/storage.h"
#include "nekos-best++.hpp"
#include "nlohmann/json.hpp"
#include <any>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <dpp/dpp.h>
#include <libpq-fe.h>
#include <mutex>
#include <regex>
#include <stdio.h>
#include <string>
#include <vector>

#define ONE_HOUR_SECOND 3600

namespace musicat
{
using json = nlohmann::json;
using string = std::string;

json sha_cfg;

bool running = true;
bool debug = false;
std::mutex main_mutex;

dpp::cluster *client_ptr = nullptr;
dpp::snowflake sha_id = 0;
std::shared_ptr<player::Manager> player_manager = nullptr;

nekos_best::endpoint_map _nekos_best_endpoints = {};

std::map<dpp::snowflake, dpp::channel> _connected_vcs_setting = {};
std::mutex _connected_vcs_setting_mutex;

dpp::cluster *
get_client_ptr ()
{
    if (!get_running_state ())
        return nullptr;
    return client_ptr;
}

dpp::snowflake
get_sha_id ()
{
    return sha_id;
}

player::player_manager_ptr
get_player_manager_ptr ()
{
    return player_manager;
}

nekos_best::endpoint_map
get_cached_nekos_best_endpoints ()
{
    return _nekos_best_endpoints;
}

int
vcs_setting_handle_connected (const dpp::channel *channel)
{
    if (!channel || !is_voice_channel (channel->get_type ()))
        return -1;
    std::lock_guard<std::mutex> lk (_connected_vcs_setting_mutex);

    _connected_vcs_setting.insert_or_assign (channel->id,
                                             dpp::channel (*channel));
    return 0;
}

int
vcs_setting_handle_updated (const dpp::channel *updated)
{
    if (!updated || !is_voice_channel (updated->get_type ()))
        return -1;
    std::lock_guard<std::mutex> lk (_connected_vcs_setting_mutex);

    _connected_vcs_setting.insert_or_assign (updated->id,
                                             dpp::channel (*updated));
    return 0;
}

int
vcs_setting_handle_disconnected (const dpp::channel *channel)
{
    if (!channel || !is_voice_channel (channel->get_type ()))
        return -1;
    std::lock_guard<std::mutex> lk (_connected_vcs_setting_mutex);

    auto i = _connected_vcs_setting.find (channel->id);

    if (i != _connected_vcs_setting.end ())
        {
            _connected_vcs_setting.erase (i);
            return 0;
        }

    return 1;
}

dpp::channel *
vcs_setting_get_cache (dpp::snowflake channel_id)
{
    std::lock_guard<std::mutex> lk (_connected_vcs_setting_mutex);
    auto i = _connected_vcs_setting.find (channel_id);

    if (i != _connected_vcs_setting.end ())
        return &i->second;

    return nullptr;
}

bool
is_voice_channel (dpp::channel_type channel_type)
{
    return channel_type == dpp::channel_type::CHANNEL_VOICE
           || channel_type == dpp::channel_type::CHANNEL_STAGE;
}

int
load_config ()
{
    std::ifstream scs ("sha_conf.json");
    if (!scs.is_open ())
        {
            fprintf (stderr, "[ERROR] No config file exist\n");
            return -1;
        }

    scs >> sha_cfg;
    scs.close ();
    return 0;
}

bool
get_running_state ()
{
    std::lock_guard<std::mutex> lk (main_mutex);
    return running;
}

int
set_running_state (const bool state)
{
    std::lock_guard<std::mutex> lk (main_mutex);
    running = state;
    return 0;
}

bool
get_debug_state ()
{
    std::lock_guard<std::mutex> lk (main_mutex);
    return debug;
}

int
set_debug_state (const bool state)
{
    std::lock_guard<std::mutex> lk (main_mutex);
    debug = state;

    printf ("[INFO] Debug mode %s\n", debug ? "enabled" : "disabled");

    return 0;
}

std::string
get_music_folder_path ()
{
    return get_config_value<std::string> ("MUSIC_FOLDER", "");
}

std::string
get_invite_link ()
{
    return get_config_value<std::string> ("INVITE_LINK", "");
}

std::string
get_oauth_link ()
{
    return get_config_value<std::string> ("OAUTH_LINK", "");
}

std::string
get_bot_description ()
{
    return get_config_value<std::string> ("DESCRIPTION", "");
}

std::string
get_webapp_dir ()
{
    return get_config_value<std::string> ("WEBAPP_DIR", "");
}

int
get_server_port ()
{
    return get_config_value<int> ("SERVER_PORT", 80);
}

void
on_sigint (int code)
{
    if (get_debug_state ())
        printf ("RECEIVED SIGINT\nCODE: %d\n", code);
    set_running_state (false);
}

void
_handle_modal_p_que_s_track (const dpp::form_submit_t &event,
                             const dpp::component &comp,
                             dpp::component &comp_2, bool &second_input)
{
    int64_t pos = 0;
    {
        if (!comp.value.index ())
            return;
        string q = std::get<string> (comp.value);
        if (!q.length ())
            return;

        sscanf (q.c_str (), "%ld", &pos);
        if (pos < 1)
            return;
    }
    bool top = comp.custom_id.find ("top") != std::string::npos;

    int64_t arg_slip = 0;
    if (second_input)
        {
            if (!comp_2.value.index ())
                return;
            string q = std::get<string> (comp_2.value);
            if (!q.length ())
                return;

            sscanf (q.c_str (), "%ld", &arg_slip);
            if (arg_slip < 1)
                return;
            if (arg_slip == 1)
                top = true;
        }

    auto storage = storage::get (event.command.message_id);
    if (!storage.has_value ())
        {
            dpp::message m ("It seems like this result is "
                            "outdated, try make a new search");
            m.flags |= dpp::m_ephemeral;
            event.reply (m);
            return;
        }

    auto tracks = std::any_cast<std::vector<yt_search::YTrack> > (storage);
    if (tracks.size () < (size_t)pos)
        return;
    auto result = tracks.at (pos - 1);

    auto from = event.from;
    auto guild_id = event.command.guild_id;

    string edit_response;
    string prepend_name;
    {
        string prepend_name = string ("<@")
                              + std::to_string (event.command.usr.id)
                              + string ("> ");
        const string prepend_response
            = top        ? " to the top of "
                           "the queue"
              : arg_slip ? " to position " + std::to_string (arg_slip)
                         : "";

        edit_response = prepend_name + string ("Added: ") + result.title ()
                        + prepend_response;
    }

    event.thinking ();
    string fname = std::regex_replace (
        result.title () + string ("-") + result.id () + string (".opus"),
        std::regex ("/"), "", std::regex_constants::match_any);
    bool dling = false;

    std::ifstream test (get_music_folder_path () + fname,
                        std::ios_base::in | std::ios_base::binary);
    if (!test.is_open ())
        {
            dling = true;
            event.edit_response (prepend_name + string ("Downloading ")
                                 + result.title ()
                                 + string ("... Gimme 10 sec ight"));
            if (player_manager->waiting_file_download.find (fname)
                == player_manager->waiting_file_download.end ())
                {
                    auto url = result.url ();
                    player_manager->download (fname, url, guild_id);
                }
        }
    else
        {
            test.close ();
            event.edit_response (edit_response);
        }

    std::thread dlt (
        [comp, prepend_name, dling, fname, guild_id, from, top, arg_slip,
         edit_response] (const dpp::interaction_create_t event,
                         yt_search::YTrack result) {
            dpp::snowflake user_id = event.command.usr.id;
            auto guild_player = player_manager->create_player (guild_id);
            if (dling)
                {
                    player_manager->wait_for_download (fname);
                    event.edit_response (edit_response);
                }
            if (from)
                guild_player->from = from;

            player::MCTrack t (result);
            t.filename = fname;
            t.user_id = user_id;
            guild_player->add_track (t, top ? true : false, guild_id, dling,
                                     arg_slip);

            if (from)
                command::play::decide_play (from, guild_id, false);
            else if (get_debug_state ())
                printf ("[modal_p] No client to "
                        "decide play\n");
        },
        event, result);
    dlt.detach ();
}

void
_handle_form_modal_p (const dpp::form_submit_t &event)
{
    if (event.components.size ())
        {
            auto comp = event.components.at (0).components.at (0);
            bool second_input = event.components.size () > 1;
            dpp::component comp_2;

            if (second_input)
                comp_2 = event.components.at (1).components.at (0);

            if (comp.custom_id.find ("que_s_track") != std::string::npos)
                {
                    _handle_modal_p_que_s_track (event, comp, comp_2,
                                                 second_input);
                }
        }
    else
        {
            fprintf (stderr, "[WARN] Form modal_p doesn't contain "
                             "any components row\n");
        }
}

int
run (int argc, const char *argv[])
{
    signal (SIGINT, on_sigint);

    // load config file
    const int config_status = load_config ();
    if (config_status != 0)
        return config_status;

    set_debug_state (get_config_value<bool> ("DEBUG", false));

    if (get_config_value<bool> ("RUNTIME_CLI", false))
        runtime_cli::attach_listener ();

    const std::string sha_token = get_config_value<string> ("SHA_TKN", "");
    if (!sha_token.length ())
        {
            fprintf (stderr, "[ERROR] No token provided\n");
            return -1;
        }

    dpp::cluster client (sha_token,
                         dpp::i_guild_members | dpp::i_default_intents);

    client_ptr = &client;

    dpp::snowflake sha_id = get_config_value<int64_t> ("SHA_ID", 0);
    if (!sha_id)
        {
            fprintf (stderr, "[ERROR] No id provided\n");
            return -1;
        }

    if (argc > 1)
        {
            int ret = cli (client, sha_id, argc, argv);
            while (running)
                std::this_thread::sleep_for (std::chrono::seconds (1));
            return ret;
        }

    const bool no_db = sha_cfg["SHA_DB"].is_null ();
    string db_connect_param = "";
    {
        if (no_db)
            {
                printf ("[WARN] No database configured, some functionality "
                        "might not work\n");
            }
        else
            {
                db_connect_param = get_config_value<string> ("SHA_DB", "");
                ConnStatusType status = database::init (db_connect_param);
                if (status != CONNECTION_OK)
                    {
                        database::shutdown ();
                        fprintf (stderr,
                                 "[ERROR] Error initializing database, code: "
                                 "%d\nSome functionality using database might "
                                 "not work\n",
                                 status);
                    }
            }
    }

    player_manager = std::make_shared<player::Manager> (&client, sha_id);

    std::function<void (const dpp::log_t &)> dpp_on_log_handler
        = dpp::utility::cout_logger ();

    client.on_log ([&dpp_on_log_handler] (const dpp::log_t &event) {
        if (!get_debug_state ())
            return;
        dpp_on_log_handler (event);
    });

    client.on_ready ([] (const dpp::ready_t &event) {
        dpp::discord_client *from = event.from;
        dpp::user me = from->creator->me;

        printf ("[READY] Shard: %d\n", from->shard_id);
        printf ("Logged in as %s#%d (%ld)\n", me.username.c_str (),
                me.discriminator, me.id);
    });

    client.on_message_create ([] (const dpp::message_create_t &event) {
        // Update channel last message Id
        dpp::channel *c = dpp::find_channel (event.msg.channel_id);
        if (c)
            c->last_message_id = event.msg.id;
    });

    client.on_button_click ([] (const dpp::button_click_t &event) {
        const size_t fsub = event.custom_id.find ("/");
        const string cmd = event.custom_id.substr (0, fsub);

        if (!cmd.length ())
            return;

        if (cmd == "page_queue")
            {
                const string param = event.custom_id.substr (fsub + 1, 1);
                if (!param.length ())
                    {
                        fprintf (
                            stderr,
                            "[WARN] command \"page_queue\" have no param\n");
                        return;
                    }
                // event.reply (dpp::ir_deferred_update_message, "");
                // dpp::message* m = new dpp::message(event.command.msg);
                paginate::update_page (event.command.msg.id, param, event);
            }
        else if (cmd == "modal_p")
            {
                const string param
                    = event.custom_id.substr (fsub + 1, string::npos);
                if (!param.length ())
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
                                    modal_enqueue_searched_track ());
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
                const string param
                    = event.custom_id.substr (fsub + 1, string::npos);
                if (!param.length ())
                    {
                        fprintf (
                            stderr,
                            "[WARN] command \"progress\" have no param\n");
                        return;
                    }

                if (param.find ("u") != std::string::npos)
                    {
                        command::progress::update_progress (event,
                                                            player_manager);
                    }
                else
                    {
                        fprintf (
                            stderr,
                            "[WARN] progress param isn't handled: \"%s\"\n",
                            param.c_str ());
                    }
            }
    });

    client.on_form_submit ([] (const dpp::form_submit_t &event) {
        if (get_debug_state ())
            printf ("[FORM] %s %ld\n", event.custom_id.c_str (),
                    event.command.message_id);
        if (event.custom_id == "modal_p")
            {
                _handle_form_modal_p (event);
            }
    });

    client.on_autocomplete ([] (const dpp::autocomplete_t &event) {
        const string cmd = event.name;
        string opt = "";
        std::vector<string> sub_cmd = {};
        string param = "";

        bool sub_level = true;

        for (const auto &i : event.options)
            {
                if (i.focused)
                    {
                        opt = i.name;
                        if (i.value.index ())
                            param = std::get<string> (i.value);
                        sub_level = false;
                        break;
                    }
            }

        int cur_sub = 0;
        std::vector<dpp::command_data_option> eopts
            = event.command.get_autocomplete_interaction ().options;

        while (sub_level && eopts.begin () != eopts.end ())
            {
                cur_sub++;
                auto sub = eopts.at (0);
                sub_cmd.push_back (sub.name);
                for (const auto &i : sub.options)
                    {
                        if (i.focused)
                            {
                                opt = i.name;
                                if (i.value.index ())
                                    param = std::get<string> (i.value);
                                sub_level = false;
                                break;
                            }
                    }
                if (!sub_level)
                    break;
                eopts = sub.options;
            }

        if (opt.length ())
            {
                if (cmd == "play")
                    {
                        if (opt == "query")
                            command::play::autocomplete::query (
                                event, param, player_manager);
                    }
                else if (cmd == "playlist")
                    {
                        if (opt == "id")
                            command::playlist::autocomplete::id (event, param);
                    }
                else if (cmd == "download")
                    {
                        if (opt == "track")
                            command::download::autocomplete::track (
                                event, param, player_manager);
                    }
                else if (cmd == "image")
                    {
                        if (opt == "type")
                            command::image::autocomplete::type (event, param);
                    }
            }
    });

    client.on_slashcommand ([] (const dpp::slashcommand_t &event) {
        if (!event.command.guild_id)
            return;

        string cmd = event.command.get_command_name ();

        if (cmd == "hello")
            command::hello::slash_run (event);
        else if (cmd == "why")
            event.reply ("Why not");
        else if (cmd == "hi")
            event.reply ("HIII");
        else if (cmd == "invite")
            command::invite::slash_run (event);
        else if (cmd == "support")
            event.reply ("https://www.discord.gg/vpk2KyKHtu");
        else if (cmd == "repo")
            event.reply ("https://github.com/Neko-Life/Musicat");
        else if (cmd == "pause")
            command::pause::slash_run (event, player_manager);
        else if (cmd == "skip")
            command::skip::slash_run (
                event,
                player_manager); // add 'force' arg, save djrole within db
        else if (cmd == "play")
            command::play::slash_run (event, player_manager);
        else if (cmd == "loop")
            command::loop::slash_run (event, player_manager);
        else if (cmd == "queue")
            command::queue::slash_run (event, player_manager);
        else if (cmd == "autoplay")
            command::autoplay::slash_run (event, player_manager);
        else if (cmd == "move")
            command::move::slash_run (event, player_manager);
        else if (cmd == "remove")
            command::remove::slash_run (event, player_manager);
        else if (cmd == "bubble_wrap")
            command::bubble_wrap::slash_run (event);
        else if (cmd == "search")
            command::search::slash_run (event);
        else if (cmd == "playlist")
            command::playlist::slash_run (event, player_manager);
        else if (cmd == "stop")
            command::stop::slash_run (event, player_manager);
        else if (cmd == "interactive_message")
            command::interactive_message::slash_run (event);
        else if (cmd == "join")
            command::join::slash_run (event, player_manager);
        else if (cmd == "leave")
            command::leave::slash_run (event, player_manager);
        else if (cmd == "download")
            command::download::slash_run (event, player_manager);
        else if (cmd == "image")
            command::image::slash_run (event);
        else if (cmd == "seek")
            command::seek::slash_run (event, player_manager);
        else if (cmd == "progress")
            command::progress::slash_run (event, player_manager);

        else
            {
                event.reply (
                    "Seems like somethin's wrong here, I can't find that "
                    "command anywhere in my database");
            }
    });

    client.on_voice_ready ([] (const dpp::voice_ready_t &event) {
        player_manager->handle_on_voice_ready (event);
    });

    client.on_voice_state_update ([] (const dpp::voice_state_update_t &event) {
        player_manager->handle_on_voice_state_update (event);
    });

    client.on_voice_track_marker ([] (const dpp::voice_track_marker_t &event) {
        if (player_manager->has_ignore_marker (event.voice_client->server_id))
            {
                if (get_debug_state ())
                    printf ("[PLAYER_MANAGER] Meta \"%s\" is ignored in guild "
                            "%ld\n",
                            event.track_meta.c_str (),
                            event.voice_client->server_id);
                return;
            }

        if (event.track_meta != "rm")
            player_manager->set_ignore_marker (event.voice_client->server_id);
        if (!player_manager->handle_on_track_marker (event, player_manager))
            {
                player_manager->delete_info_embed (
                    event.voice_client->server_id);
            }

        // track marker remover
        std::thread t ([event] () {
            if (!event.voice_client)
                return;
            short int count = 0;
            const bool d_s = get_debug_state ();

            while (!event.voice_client->terminating
                   && !event.voice_client->is_playing ()
                   && !event.voice_client->is_paused ())
                {
                    std::this_thread::sleep_for (
                        std::chrono::milliseconds (500));
                    count++;
                    if (count == 10)
                        break;
                }

            player_manager->remove_ignore_marker (
                event.voice_client->server_id);
            if (!d_s)
                return;

            printf ("Removed ignore marker for meta '%s'",
                    event.track_meta.c_str ());
            if (event.voice_client)
                printf (" in %ld", event.voice_client->server_id);
            printf ("\n");
        });

        t.detach ();
    });

    client.on_message_delete ([] (const dpp::message_delete_t &event) {
        player_manager->handle_on_message_delete (event);
        paginate::handle_on_message_delete (event);
    });

    client.on_message_delete_bulk (
        [] (const dpp::message_delete_bulk_t &event) {
            player_manager->handle_on_message_delete_bulk (event);
            paginate::handle_on_message_delete_bulk (event);
        });

    client.on_channel_update ([] (const dpp::channel_update_t &event) {
        const bool debug = get_debug_state ();

        // reconnect if has different vc region
        if (event.updated && is_voice_channel (event.updated->get_type ()))
            {
                dpp::channel *cached
                    = vcs_setting_get_cache (event.updated->id);

                if (cached
                    && (cached->rtc_region != event.updated->rtc_region))
                    {
                        dpp::snowflake guild_id = event.updated->guild_id;

                        // set to seek to last position in the next playing
                        // track !TODO: probably add this to player manager for
                        // convenience?
                        auto guild_player
                            = player_manager->get_player (guild_id);
                        if (guild_player && guild_player->queue.size ())
                            {
                                int64_t to_seek
                                    = guild_player->current_track.current_byte
                                      - (BUFSIZ * 8);
                                if (to_seek < 0)
                                    to_seek = 0;

                                guild_player->queue.front ().seek_to = to_seek;
                            }

                        // rejoin channel
                        if (debug)
                            fprintf (stderr, "[update_rtc_region] %ld\n",
                                     cached->id);

                        auto *from = event.from;
                        dpp::snowflake channel_id = event.updated->id;

                        player_manager->full_reconnect (
                            from, guild_id, channel_id, channel_id);
                    }
                // !TODO: check for updated voice region
            }

        // update vc cache
        vcs_setting_handle_updated (event.updated);
    });

    // client.set_websocket_protocol(dpp::websocket_protocol_t::ws_etf);

    _nekos_best_endpoints = nekos_best::get_available_endpoints ();
    client.start (true);

    // start server
    std::thread server_thread ([] () { server::run (); });

    server_thread.detach ();

    time_t last_gc;
    time_t last_recon;
    time (&last_gc);
    time (&last_recon);

    while (get_running_state ())
        {
            std::this_thread::sleep_for (std::chrono::seconds (1));

            const bool r_s = get_running_state ();
            const bool d_s = get_debug_state ();
            // GC
            if (!r_s || (time (NULL) - last_gc) > ONE_HOUR_SECOND)
                {
                    if (d_s)
                        printf ("[GC] Starting scheduled gc\n");
                    auto start_time
                        = std::chrono::high_resolution_clock::now ();
                    // gc codes
                    paginate::gc (!running);

                    // reset last_gc
                    time (&last_gc);

                    auto end_time = std::chrono::high_resolution_clock::now ();
                    auto done = std::chrono::duration_cast<
                        std::chrono::milliseconds> (end_time - start_time);
                    if (d_s)
                        printf ("[GC] Ran for %ld ms\n", done.count ());
                }

            if (r_s && !no_db && (time (NULL) - last_recon) > 60)
                {
                    const ConnStatusType status
                        = database::reconnect (false, db_connect_param);
                    time (&last_recon);

                    if (status != CONNECTION_OK && d_s)
                        fprintf (stderr,
                                 "[ERROR DB_RECONNECT] Status code: %d\n",
                                 status);
                }
        }

    database::shutdown ();

    return 0;
}
} // musicat

// vim: et sw=4 ts=8
