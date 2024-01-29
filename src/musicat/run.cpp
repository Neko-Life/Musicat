/*
    Global program states goes here
*/

#include "musicat/child.h"
#include "musicat/child/ytdlp.h"
#include "musicat/db.h"
#include "musicat/eliza.h"
#include "musicat/events.h"
#include "musicat/function_macros.h"
#include "musicat/musicat.h"
#include "musicat/pagination.h"
#include "musicat/player_manager_timer.h"
#include "musicat/runtime_cli.h"
#include "musicat/server.h"
#include "musicat/thread_manager.h"

static const std::string OAUTH_BASE_URL
    = "https://discord.com/api/oauth2/authorize";

namespace musicat
{
nlohmann::json sha_cfg; // EXTERN_VARIABLE

std::atomic<bool> running = false;
bool debug = false;
std::mutex main_mutex;

dpp::snowflake sha_id = 0;
dpp::cluster *client_ptr = nullptr;
player::Manager *player_manager_ptr = nullptr;

nekos_best::endpoint_map _nekos_best_endpoints = {};

std::map<dpp::snowflake, dpp::channel> _connected_vcs_setting = {};
std::mutex _connected_vcs_setting_mutex;
float _stream_buffer_size = 0.0f;

int python_v = -1;

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
    if (sha_id)
        return sha_id;

    sha_id = get_config_value<int64_t> ("SHA_ID", 0);

    return sha_id;
}

std::string
get_sha_token ()
{
    return get_config_value<std::string> ("SHA_TKN", "");
}

std::string
get_sha_secret ()
{
    return get_config_value<std::string> ("SHA_SECRET", "");
}

std::string
get_sha_db_params ()
{
    return get_config_value<std::string> ("SHA_DB", "");
}

bool
get_sha_runtime_cli_opt ()
{
    return get_config_value<bool> ("RUNTIME_CLI", false);
}

player::player_manager_ptr_t
get_player_manager_ptr ()
{
    if (!get_running_state ())
        return nullptr;

    return player_manager_ptr;
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
load_config (const std::string &config_file)
{
    std::ifstream scs (config_file);
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

    fprintf (stderr, "[INFO] Debug mode %s\n", debug ? "enabled" : "disabled");

    return 0;
}

std::string
get_music_folder_path ()
{
    return get_config_value<std::string> ("MUSIC_FOLDER", "");
}

std::string
get_invite_oauth_base_url ()
{
    dpp::snowflake sid = get_sha_id ();

    if (!sid)
        return "";

    std::string sha_id_str = std::to_string (sid);

    return OAUTH_BASE_URL + "?client_id=" + sha_id_str;
}

std::string
get_invite_permissions ()
{
    return get_config_value<std::string> ("INVITE_PERMISSIONS", "");
}

std::string
get_invite_scopes ()
{
    return get_config_value<std::string> ("INVITE_SCOPES", "");
}

std::string
get_oauth_scopes ()
{
    return get_config_value<std::string> ("OAUTH_SCOPES", "");
}

std::string
get_default_oauth_params ()
{
    return "&prompt=none&response_type=code";
}

std::string
construct_permissions_param (const std::string permissions)
{
    if (permissions.empty ())
        return "";

    return "&permissions=" + permissions;
}

std::string
construct_scopes_param (const std::string scopes)
{
    if (scopes.empty ())
        return "";

    return "&scope=" + encodeURIComponent (scopes);
}

std::string
get_invite_link ()
{
    const std::string append_str
        = construct_permissions_param (get_invite_permissions ())
          + construct_scopes_param (get_invite_scopes ());

    if (append_str.empty ())
        return "";

    return get_invite_oauth_base_url () + append_str;
}

std::string
get_oauth_link ()
{
    const std::string append_str
        = construct_scopes_param (get_oauth_scopes ());

    if (append_str.empty ())
        return "";

    return get_invite_oauth_base_url () + get_default_oauth_params ()
           + append_str;
}

std::string
get_oauth_invite ()
{
    const std::string invite_scopes = get_invite_scopes ();
    const std::string oauth_scopes = get_oauth_scopes ();

    const std::string append_str
        = construct_permissions_param (get_invite_permissions ())
          + ((!invite_scopes.empty () && !oauth_scopes.empty ())
                 ? construct_scopes_param (invite_scopes + " " + oauth_scopes)
                 : "");

    if (append_str.empty ())
        return "";

    return get_invite_oauth_base_url () + get_default_oauth_params ()
           + append_str;
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

std::string
get_ytdlp_exe ()
{
    return get_config_value<std::string> ("YTDLP_EXE", "");
}

std::string
get_ytdlp_util_exe ()
{
    return get_config_value<std::string> ("YTDLP_UTIL_EXE", "");
}

std::string
get_ytdlp_lib_path ()
{
    return get_config_value<std::string> ("YTDLP_LIB_DIR", "");
}

std::vector<std::string>
get_cors_enabled_origins ()
{
    std::vector<std::string> ret = {};

    auto i_a = sha_cfg.find ("CORS_ENABLED_ORIGINS");

    if (i_a == sha_cfg.end () || !i_a->is_array () || !i_a->size ())
        return ret;

    size_t i_a_siz = i_a->size ();

    ret.reserve (i_a_siz);

    for (size_t i = 0; i < i_a_siz; i++)
        {
            nlohmann::json entry = i_a->at (i);

            if (!entry.is_string () || !entry.size ())
                continue;

            ret.push_back (entry.get<std::string> ());
        }

    return ret;
}

std::string
get_jwt_secret ()
{
    return get_config_value<std::string> ("JWT_SECRET", "");
}

float
get_stream_buffer_size ()
{
    if (_stream_buffer_size < 0.1f)
        {
            float set_v = get_config_value<float> ("STREAM_BUFFER_SIZE", 0.0f);

            if (set_v < 0.1f)
                set_v = 0.3f;

            _stream_buffer_size = set_v;
        }

    return _stream_buffer_size;
}

const char *
get_python_cmd ()
{
    switch (python_v)
        {
        case 3:
            return "python3";
        case 2:
            return "python2";
        case 1:
            return "python";
        default:
            return "";
        }
}

std::atomic<int> _sigint_count = 0;

void
on_sigint ([[maybe_unused]] int code)
{
    _sigint_count++;

    static const char exit_msg[] = "Received SIGINT, exiting...\n";

    // printf isn't signal safe, use write
    write (STDERR_FILENO, exit_msg, STR_SIZE (exit_msg));

    if (_sigint_count > 1)
        {
            static const char stuck_help[]
                = "If the program seems stuck try type something into your "
                  "terminal and then press ENTER\n";

            write (STDERR_FILENO, stuck_help, STR_SIZE (stuck_help));
        }

    if (_sigint_count >= 5)
        {
            static const char force_exit_msg[]
                = "Understood, force exiting...\n";

            write (STDERR_FILENO, force_exit_msg, STR_SIZE (force_exit_msg));

            exit (255);
        }

    if (running)
        running = false;
}

auto dpp_cout_logger = dpp::utility::cout_logger ();

int
run (int argc, const char *argv[])
{
    signal (SIGINT, on_sigint);
    set_running_state (true);

    // load config file
    int config_status = load_config ();
    if (config_status != 0)
        return config_status;

    set_debug_state (get_config_value<bool> ("DEBUG", false));

    const std::string sha_token = get_sha_token ();
    if (sha_token.empty ())
        {
            fprintf (stderr, "[ERROR] No token provided\n");
            return -1;
        }

    get_sha_id ();

    if (!sha_id)
        {
            fprintf (stderr, "[ERROR] No bot user Id provided\n");
            return -1;
        }

    const musicat_cluster_params_t cluster_params
        = { sha_token,
            dpp::i_guild_members | dpp::i_default_intents,
            0,
            0,
            1,
            true,
            dpp::cache_policy::cpol_default,
            1,
            1 };

    if (argc > 1)
        {
            dpp::cluster client (
                cluster_params.token, cluster_params.intents,
                cluster_params.shards, cluster_params.cluster_id,
                cluster_params.maxclusters, cluster_params.compressed,
                cluster_params.policy, cluster_params.request_threads,
                cluster_params.request_threads_raw);

            client_ptr = &client;

            int ret = cli (client, sha_id, argc, argv);

            while (running)
                std::this_thread::sleep_for (std::chrono::seconds (1));

            return ret;
        }

    if ((python_v = child::ytdlp::has_python ()) == -1)
        {
            fprintf (stderr,
                     "[FATAL] Unable to invoke python, this program requires "
                     "python to use yt-dlp capabilities, exiting...\n");

            return -1;
        }

    if (eliza::init () != 0)
        {
            fprintf (stderr,
                     "[ERROR] Something wrong when initializing Eliza...\n");
        }

    // no return after this, init child
    const int child_init_status = child::init ();
    if (child_init_status)
        {
            fprintf (stderr, "[FATAL] Can't initialize child, exiting...\n");
            return child_init_status;
        }

    // !IMPORTANT: only AFTER initializing child can you
    // spawn a thread! Never fork after being multi threaded!
    if (get_sha_runtime_cli_opt ())
        runtime_cli::attach_listener ();

    const bool no_db = sha_cfg["SHA_DB"].is_null ();
    std::string db_connect_param = "";

    if (no_db)
        {
            fprintf (stderr, "[WARN] No database configured, some "
                             "functionality might not work\n");
        }
    else
        {
            db_connect_param = get_sha_db_params ();
            ConnStatusType status = database::init (db_connect_param);
            if (status != CONNECTION_OK)
                {
                    database::shutdown ();
                    fprintf (
                        stderr,
                        "[ERROR] Error initializing database, code: %d\n"
                        "Some functionality using database might not work\n",
                        status);
                }
        }

    // initialize cluster here since constructing cluster
    // also spawns threads
    dpp::cluster client (cluster_params.token, cluster_params.intents,
                         cluster_params.shards, cluster_params.cluster_id,
                         cluster_params.maxclusters, cluster_params.compressed,
                         cluster_params.policy, cluster_params.request_threads,
                         cluster_params.request_threads_raw);

    player::Manager player_manager (&client);

    client_ptr = &client;
    player_manager_ptr = &player_manager;

    client.on_log ([] (const dpp::log_t &event) {
        if (!get_debug_state ())
            return;

        dpp_cout_logger (event);
        fprintf (stderr, "%s\n", event.raw_event.c_str ());
    });

    events::load_events (client_ptr);

#ifdef MUSICAT_WS_P_ETF
    client.set_websocket_protocol (dpp::websocket_protocol_t::ws_etf);
#endif

    _nekos_best_endpoints = nekos_best::get_available_endpoints ();
    client.start (true);

    // start server
    std::thread server_thread ([] () {
        thread_manager::DoneSetter tmds;

        server::run ();
    });

    thread_manager::dispatch (server_thread);

    time_t last_gc;
    time_t last_recon;
    time (&last_gc);
    time (&last_recon);

    while (get_running_state ())
        {
            std::this_thread::sleep_for (std::chrono::milliseconds (750));

            bool r_s = get_running_state ();
            bool debug = get_debug_state ();

            // GC
            if (!r_s || (time (NULL) - last_gc) > ONE_HOUR_SECOND)
                {
                    if (debug)
                        fprintf (stderr, "[GC] Starting scheduled gc\n");

                    auto start_time
                        = std::chrono::high_resolution_clock::now ();

                    // gc codes
                    paginate::gc (!running);

                    // reset last_gc
                    time (&last_gc);

                    auto end_time = std::chrono::high_resolution_clock::now ();
                    auto done = std::chrono::duration_cast<
                        std::chrono::milliseconds> (end_time - start_time);

                    if (debug)
                        fprintf (stderr, "[GC] Ran for %lld ms\n",
                                 done.count ());
                }

            if (r_s && !no_db && (time (NULL) - last_recon) > 5)
                {
                    ConnStatusType status
                        = database::reconnect (false, db_connect_param);

                    time (&last_recon);

                    if (status != CONNECTION_OK && debug)
                        fprintf (stderr,
                                 "[ERROR DB_RECONNECT] Status code: %d\n",
                                 status);
                }

            player::timer::check_track_marker_rm_timers ();
            player::timer::check_resume_timers ();

            server::main_loop_routine ();

            thread_manager::join_done ();
        }

    child::shutdown ();

    server::shutdown ();
    client.shutdown ();

    client_ptr = nullptr;

    thread_manager::join_all ();
    database::shutdown ();

    return 0;
}

} // musicat

// vim: et sw=4 ts=8
