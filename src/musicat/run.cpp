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
#include <cstdint>
#include <sys/wait.h>

#define RUN_TESTS 0

#if RUN_TESTS
#include "musicat/tests.h"
#endif

static const std::string OAUTH_BASE_URL
    = "https://discord.com/api/oauth2/authorize";

namespace musicat
{
nlohmann::json sha_cfg; // EXTERN_VARIABLE

// ================================================================================

// use this mutex for every state stored here
std::mutex main_mutex;

std::atomic<bool> running = false;
bool debug = false;

dpp::snowflake sha_id = 0;
dpp::cluster *client_ptr = nullptr;
player::Manager *player_manager_ptr = nullptr;

nekos_best::endpoint_map _nekos_best_endpoints = {};

// config stuff ========================================

std::string music_folder_path = "";

// in second
float _stream_buffer_size = 0.0f;
int64_t _stream_sleep_on_buffer_threshold_ms = 0;

int python_v = -1;

std::vector<std::string> cors_enabled_origin = {};
std::vector<dpp::snowflake> musicat_admins = {};

size_t max_music_cache_size = -1;

// main loop usage only
std::atomic<bool> should_check_music_cache = true;

// ================================================================================

// doesn't guarantee valid voicestate
std::map<dpp::snowflake, std::pair<dpp::channel, dpp::voicestate> >
    _connected_vcs_setting = {};
std::mutex _connected_vcs_setting_mutex;

// ================================================================================

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

int
set_cached_nekos_best_endpoints (const nekos_best::endpoint_map &em)
{
    std::lock_guard lk (main_mutex);
    _nekos_best_endpoints = em;
    return 0;
}

nekos_best::endpoint_map
get_cached_nekos_best_endpoints ()
{
    std::lock_guard lk (main_mutex);
    return _nekos_best_endpoints;
}

int
vcs_setting_handle_connected (const dpp::channel *channel,
                              const dpp::voicestate *state)
{
    if (!channel || !state || !is_voice_channel (channel->get_type ()))
        return -1;

    std::lock_guard lk (_connected_vcs_setting_mutex);

    _connected_vcs_setting.insert_or_assign (
        channel->id,
        std::make_pair (dpp::channel (*channel), dpp::voicestate (*state)));
    return 0;
}

int
vcs_setting_handle_updated (const dpp::channel *updated,
                            dpp::voicestate *state)
{
    if (!updated || !is_voice_channel (updated->get_type ()))
        return -1;

    std::lock_guard lk (_connected_vcs_setting_mutex);

    bool prev_state = false;
    if (!state)
        {
            auto i = _connected_vcs_setting.find (updated->id);
            if (i != _connected_vcs_setting.end ())
                {
                    state = &i->second.second;
                    prev_state = true;
                }
        }

    _connected_vcs_setting.insert_or_assign (
        updated->id, std::make_pair (dpp::channel (*updated),
                                     prev_state ? std::move (*state)
                                     : state    ? dpp::voicestate (*state)
                                                : dpp::voicestate ()));

    return 0;
}

int
vcs_setting_handle_disconnected (const dpp::channel *channel)
{
    if (!channel || !is_voice_channel (channel->get_type ()))
        return -1;

    std::lock_guard lk (_connected_vcs_setting_mutex);

    auto i = _connected_vcs_setting.find (channel->id);

    if (i != _connected_vcs_setting.end ())
        {
            _connected_vcs_setting.erase (i);
            return 0;
        }

    return 1;
}

std::pair<dpp::channel *, dpp::voicestate *>
vcs_setting_get_cache (dpp::snowflake channel_id)
{
    std::lock_guard lk (_connected_vcs_setting_mutex);
    auto i = _connected_vcs_setting.find (channel_id);

    if (i != _connected_vcs_setting.end ())
        return std::make_pair (&i->second.first, &i->second.second);

    return std::make_pair (nullptr, nullptr);
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
    std::lock_guard lk (main_mutex);
    return running;
}

int
set_running_state (const bool state)
{
    std::lock_guard lk (main_mutex);
    running = state;
    return 0;
}

bool
get_debug_state ()
{
    std::lock_guard lk (main_mutex);
    return debug;
}

int
set_debug_state (const bool state)
{
    std::lock_guard lk (main_mutex);
    debug = state;

    fprintf (stderr, "[INFO] Debug mode %s\n", debug ? "enabled" : "disabled");

    return 0;
}

std::string
get_music_folder_path ()
{
    std::lock_guard lk (main_mutex);

    const bool is_empty = music_folder_path.empty ();

    if (!is_empty && music_folder_path == "0")
        {
            return "";
        }

    if (is_empty)
        {
            music_folder_path
                = get_config_value<std::string> ("MUSIC_FOLDER", "");

            if (music_folder_path.empty ())
                {
                    music_folder_path = "0";
                }
            else if (*(music_folder_path.end () - 1) != '/')
                {
                    fprintf (stderr,
                             "[get_music_folder_path ERROR] MUSIC_FOLDER must "
                             "have a trailing slash '/'\n");

                    std::terminate ();
                }
        }

    return music_folder_path;
}

std::string
get_invite_oauth_base_url ()
{
    dpp::snowflake sid = get_sha_id ();

    if (!sid)
        return "";

    return OAUTH_BASE_URL + "?client_id=" + sid.str ();
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
    return cors_enabled_origin;
}

void
load_config_cors_enabled_origin ()
{
    std::lock_guard lk (main_mutex);

    auto i_a = sha_cfg.find ("CORS_ENABLED_ORIGINS");

    if (i_a == sha_cfg.end () || !i_a->is_array () || !i_a->size ())
        return;

    size_t i_a_siz = i_a->size ();

    cors_enabled_origin.reserve (i_a_siz);

    for (size_t i = 0; i < i_a_siz; i++)
        {
            nlohmann::json entry = i_a->at (i);

            if (!entry.is_string () || !entry.size ())
                continue;

            cors_enabled_origin.push_back (entry.get<std::string> ());
        }
}

std::string
get_jwt_secret ()
{
    return get_config_value<std::string> ("JWT_SECRET", "");
}

float
get_stream_buffer_size ()
{
    std::lock_guard lk (main_mutex);

    if (_stream_buffer_size < 0.1f)
        {
            float set_v = get_config_value<float> ("STREAM_BUFFER_SIZE", 0.0f);

            if (set_v < 0.1f)
                set_v = 0.3f;

            _stream_buffer_size = set_v;
        }

    return _stream_buffer_size;
}

int64_t
get_stream_sleep_on_buffer_threshold_ms ()
{
    std::lock_guard lk (main_mutex);

    if (_stream_sleep_on_buffer_threshold_ms < 1)
        {
            int64_t set_v = get_config_value<int64_t> (
                "STREAM_SLEEP_ON_BUFFER_THRESHOLD_MS", 0);

            if (set_v < 1)
                set_v = 1;

            _stream_sleep_on_buffer_threshold_ms = set_v;
        }

    return _stream_sleep_on_buffer_threshold_ms;
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

bool
is_musicat_admin (const dpp::snowflake &id)
{
    std::lock_guard lk (main_mutex);

    for (const auto &s : musicat_admins)
        {
            if (id == s)
                return true;
        }

    return false;
}

void
load_config_admins ()
{
    auto i_a = sha_cfg.find ("ADMIN_IDS");

    if (i_a == sha_cfg.end () || !i_a->is_array () || !i_a->size ())
        return;

    std::lock_guard lk (main_mutex);

    for (const auto &a : *i_a)
        {
            musicat_admins.push_back (a.get<uint64_t> ());
        }
}

void
set_should_check_music_cache (bool v)
{
    should_check_music_cache = v;
}

size_t
get_max_music_cache_size ()
{
    std::lock_guard lk (main_mutex);

    if (max_music_cache_size == (size_t)-1)
        {
            size_t set_v
                = get_config_value<size_t> ("MAX_MUSIC_CACHE_SIZE", (size_t)0);

            max_music_cache_size = set_v;
        }

    return max_music_cache_size;
}

// ================================================================================

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

    load_config_admins ();
    load_config_cors_enabled_origin ();

    const musicat_cluster_params_t cluster_params
        = { sha_token,
            dpp::i_guild_members | dpp::i_default_intents,
            0,
            0,
            1,
            true,
            dpp::cache_policy::cpol_default,
            12,
            4 };

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

#ifdef MC_EX_VC_REC
    client.on_voice_receive ([] (const dpp::voice_receive_t &event) {
        std::cout << "[voice_receive]:\n" << event.raw_event;
        std::cout << "\n:[voice_receive]\n";
    });

    client.on_voice_receive_combined ([] (const dpp::voice_receive_t &event) {
        std::cout << "[voice_receive_combined]:\n" << event.raw_event;
        std::cout << "\n:[voice_receive_combined]\n";
    });
#endif

#ifdef MUSICAT_WS_P_ETF
    client.set_websocket_protocol (dpp::websocket_protocol_t::ws_etf);
#endif

    try
        {
            set_cached_nekos_best_endpoints (
                nekos_best::get_available_endpoints ());
        }
    catch (const std::exception &e)
        {
            fprintf (stderr,
                     "[ERROR] nekos_best::get_available_endpoints:\n%s\n",
                     e.what ());
        }
    client.start (true);

    // start server
    std::thread server_thread ([] () {
        thread_manager::DoneSetter tmds;

        server::run ();
    });

    thread_manager::dispatch (server_thread);

#if RUN_TESTS
    tests::test_ytdlp ();
#endif

    time_t last_gc;
    time_t last_5sec;
    time_t last_music_cache_check = 0;

    time (&last_gc);
    time (&last_5sec);

    bool r_s;
    while ((r_s = get_running_state ()))
        {
            std::this_thread::sleep_for (std::chrono::milliseconds (750));

            bool debug = get_debug_state ();
            time_t cur_time = time (NULL);

            // GC
            if (!r_s || (cur_time - last_gc) > ONE_HOUR_SECOND)
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

            if (r_s && (cur_time - last_5sec) > 5)
                {
                    // db reconnect check
                    if (!no_db)
                        {
                            ConnStatusType status = database::reconnect (
                                false, db_connect_param);

                            if (status != CONNECTION_OK && debug)
                                fprintf (
                                    stderr,
                                    "[ERROR DB_RECONNECT] Status code: %d\n",
                                    status);
                        }

                    const size_t mcs = get_max_music_cache_size ();
                    if ((should_check_music_cache
                         || ((cur_time - last_music_cache_check)
                             > ONE_HOUR_SECOND))
                        && mcs != 0)
                        {
                            player::control_music_cache (mcs);

                            should_check_music_cache = false;
                            last_music_cache_check = cur_time;
                        }

                    // the only solution to reap child exited abnormally in
                    // docker container env
                    int wstatus;
                    while (waitpid (-1, &wstatus, WNOHANG) > 0)
                        ;

                    time (&last_5sec);
                }

            player::timer::check_track_marker_rm_timers ();
            player::timer::check_resume_timers ();
            player::timer::check_failed_playback_reset_timers ();

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
