// clang-format off
#include "musicat/mctrack.h"
#include "musicat/player.h"
#include "musicat/player_manager.h"
#include "musicat/YTDLPTrack.h"
// clang-format on

#include "musicat/player_manager_util.h"
#include "musicat/child/command.h"
#include "musicat/child/dl_music.h"
#include "musicat/db.h"
#include "musicat/musicat.h"
#include "musicat/player_manager_timer.h"
#include "musicat/server/ws/player.h"
#include "musicat/task.h"
#include "musicat/util.h"
#include "musicat/util/base64.h"
#include "musicat/util/fs.h"
#include "musicat/util_response.h"

#ifdef MUSICAT_WITH_PYTHON
#include "musicat/ytdlp.h"
#endif // MUSICAT_WITH_PYTHON

#include <cstdint>
#include <dirent.h>
#include <exception>
#include <libpq-fe.h>
#include <regex>
#include <sys/stat.h>
#include <utime.h>

/* #define USE_SEARCH_CACHE */
// #define TEST_NO_AUTOPAUSE

#define ENABLE_DAVE true
#define SELF_DEAF true

namespace musicat
{
namespace player
{

enum gat_cache_completeness_e
{
    GATCC_NONE = 0,
    GATCC_ZERO_AMOUNT = 1,
    GATCC_HAS_STAT = 1 << 1,
};

static std::mutex gat_m;
static std::vector<gat_t> gat_ret_cache;
static time_t gat_last_uc = 0;
static int gat_cache_completeness = 0;

// returns true if gat_cache_completeness is missing flag
// and required by rc
static bool
gatcc_rnot (int rc, int flag)
{
    return (rc & flag) == flag && (gat_cache_completeness & flag) != flag;
}

static bool
gatcc_is_complete ()
{
    constexpr const int complete_f = GATCC_ZERO_AMOUNT | GATCC_HAS_STAT;
    return (gat_cache_completeness & complete_f) == complete_f;
}

static bool
gatcc_rnot_zero_amount (int rc)
{
    return gatcc_rnot (rc, GATCC_ZERO_AMOUNT);
}

static bool
gatcc_rnot_has_stat (int rc)
{
    return gatcc_rnot (rc, GATCC_HAS_STAT);
}

static void
invalidate_track_list_cache ()
{
    std::lock_guard lk (gat_m);
    gat_last_uc = 0;
}

std::vector<gat_t>
get_available_tracks (const size_t &amount, bool with_stat)
{
    std::lock_guard lk (gat_m);

    int required_completeness = 0;

    if (amount == 0)
        required_completeness |= GATCC_ZERO_AMOUNT;

    if (with_stat)
        required_completeness |= GATCC_HAS_STAT;

    time_t cur_t = time (NULL);

    // cache last updated was less than 11 second ago
    if ((cur_t - gat_last_uc) < 11)
        {
            // check for cache completeness
            bool is_sufficient = true;

            if (!gatcc_is_complete ())
                is_sufficient = !gatcc_rnot_zero_amount (required_completeness) && !gatcc_rnot_has_stat (required_completeness);

            // returns cache if sufficient
            if (is_sufficient)
                return gat_ret_cache;
        }

    std::vector<gat_t> ret = {};
    const std::string musicdir = get_music_folder_path ();

    size_t c = 0;
    DIR *dir = opendir (musicdir.c_str ());

    if (dir == NULL)
        return ret;

    dirent *file = readdir (dir);

    // full filename
    std::string s;
    // full file path
    std::string fpath;
    size_t slen;
    size_t opus_ext_idx;
    // while also getting size why not get last access too
    time_t last_access = 0;
    size_t siz = 0;

    while (file != NULL)
        {
            last_access = 0;
            siz = 0;

            // skip non regular file (dir, fifo etc)
            if (file->d_type != DT_REG)
                goto cont;

            s = std::string (file->d_name);

            slen = s.length ();
            opus_ext_idx = slen > 5 ? (slen - 5) : (size_t)-1;

            // skip non opus file
            if (opus_ext_idx == (size_t)-1 || s.find (".opus") != opus_ext_idx)
                goto cont;

            fpath = musicdir + s;

            if (with_stat)
                {
                    struct stat st;
                    // musicdir always have trailing slash /
                    if (stat (fpath.c_str (), &st) != 0)
                        {
                            perror ("[player::get_available_tracks] stat");

                            fprintf (stderr, "^^^ Failed stating '%s'\n", fpath.c_str ());

                            goto sk_stat;
                        }

                    siz = st.st_size;
                    last_access = st.st_atime;
                }
        sk_stat:
            ret.push_back ({ s.substr (0, opus_ext_idx), s, fpath, siz, last_access });

            if (amount && ++c == amount)
                break;

        cont:
            file = readdir (dir);
        }

    closedir (dir);

    gat_ret_cache = ret;
    gat_last_uc = cur_t;
    gat_cache_completeness = required_completeness;

    return ret;
}

void
control_music_cache (const size_t size_limit)
{
    if (size_limit == 0)
        return;

    size_t cur_cache_size = 0;
    auto ats = get_available_tracks (0, true);

    for (const gat_t &g : ats)
        cur_cache_size += g.size;

    fprintf (stderr, "[main::loop] Current cached music: %ld files (%ld bytes)\n", ats.size (), cur_cache_size);

    if (cur_cache_size <= size_limit)
        // current cached music size does not go over limit
        return;

    // currect cached music is over the limit defined
    // in conf

    fprintf (stderr, "[main::loop] Current cached music size goes over %ld bytes limit, cleaning old music...\n", size_limit);

    // sort by oldest first
    std::sort (ats.begin (), ats.end (), [] (const gat_t &a, const gat_t &b) { return a.last_access < b.last_access; });

    size_t prev_cache_size = cur_cache_size;
    size_t rc = 0;

    // delete everything until size is less
    // than limit
    for (const gat_t &g : ats)
        {
            if (cur_cache_size < size_limit)
                break;

            fprintf (stderr, "[main::loop] Unlinking '%s'\n", g.fullpath.c_str ());

            if (unlink (g.fullpath.c_str ()) == 0)
                {
                    cur_cache_size -= g.size;
                    rc++;
                    continue;
                }

            perror ("[main::loop] unlink");
            fprintf (stderr, "^^^ Failed unlink '%s'\n", g.fullpath.c_str ());
        }

    fprintf (stderr, "[main::loop] Cleaned up %ld music files and %ld bytes worth of storage space\n", rc,
             prev_cache_size - cur_cache_size);

    invalidate_track_list_cache ();
}

// ================================================================================

static std::mutex tfpc_m;
static std::map<std::string, int> track_failed_playback_counts;

int
get_track_failed_playback_count (const std::string &filename)
{
    if (filename.empty ())
        {
            if (get_debug_state ())
                {
                    fprintf (stderr, "[player::get_track_failed_playback_count ERROR] track.filename is empty\n");
                }

            return -1;
        }

    std::lock_guard lk (tfpc_m);

    auto i = track_failed_playback_counts.find (filename);
    if (i == track_failed_playback_counts.end ())
        {
            return 0;
        }

    return i->second;
}

int
set_track_failed_playback_count (const std::string &filename, int c)
{
    const bool debug = get_debug_state ();

    if (filename.empty ())
        {
            if (debug)
                {
                    fprintf (stderr, "[player::set_track_failed_playback_count ERROR] `track.filename` is empty\n");
                }

            return -1;
        }

    if (c < 0)
        {
            if (debug)
                {
                    fprintf (stderr, "[player::set_track_failed_playback_count ERROR] `c` can't be less than 0\n");
                }

            return -2;
        }

    std::lock_guard lk (tfpc_m);

    if (c == 0)
        track_failed_playback_counts.erase (filename);
    else
        track_failed_playback_counts.insert_or_assign (filename, c);

    return 0;
}

// ================================================================================

bool
find_track_will_block ()
{
    return mctrack::fetch_will_block ();
}

std::pair<player::MCTrack, int>
find_track (const bool playlist, const std::string &arg_query, const dpp::snowflake guild_id, const bool no_check_history,
            const std::string &cache_id)
{
    std::string trimmed_query = util::trim_str (arg_query);

#ifdef USE_SEARCH_CACHE
    bool has_cache_id = !cache_id.empty ();
#else
    // bool has_cache_id = false;
#endif

    std::shared_ptr<player::guild_player_t> guild_player = NULL;

    // i wonder what was this for... i do the one who wrote this but i forgot
    if (playlist && !no_check_history)
        {
            guild_player = manager::get_player (guild_id);

            if (!guild_player)
                return { {}, 1 };

            // if there's no track return without searching first?
            auto lk = guild_player->acquire ();
            if (guild_player->queue.begin () == guild_player->queue.end ())
                {
                    return { {}, 1 };
                }
        }

    // prioritize cache over searching
    std::vector<player::MCTrack> searches;

#ifdef USE_YTSEARCH_H
    yt_search::YSearchResult search_result = {};
    yt_search::YPlaylist playlist_result = {};

#ifdef USE_SEARCH_CACHE
    if (has_cache_id)
        searches = search_cache::get (cache_id);
#endif

    size_t searches_size = has_cache_id ? searches.size () : 0;

    // quick decide to remove when no result found instead of looking up in the
    // cache map
    size_t cached_size = has_cache_id ? searches_size : 0;

    bool searched = false;

    // cache not found or no cache Id provided, lets search
    if (!searches_size)
        {
            try
                {
                    searches = playlist ? (playlist_result = yt_search::get_playlist (trimmed_query)).entries ()

                                        : (search_result = yt_search::search (trimmed_query)).trackResults ();

                    searches_size = searches.size ();

                    if (!playlist && !searches_size)
                        // desperate to get a track
                        // get_playlist already do this if no track from
                        // default result found
                        searches = search_result.sideTrackPlaylist ();

                    searched = true;
                }
            catch (std::exception &e)
                {
                    std::cerr << "[player::find_track ERROR] " << guild_id << ':' << e.what () << '\n';

                    return { {}, 1 };
                }
        }

    if (playlist && playlist_result.status == 1 && !searches.empty ())
        {
            // remove duplicate track as first sideTrackPlaylist entry is a
            // duplicate of the searched track
            searches.erase (searches.begin ());
        }

    searches_size = searches.size ();

#else
    // use mctrack::fetch
    // playlist true means autoplay request, which is always a playlist url
    // query
    nlohmann::json res = mctrack::fetch ({ trimmed_query, YDLP_DEFAULT_MAX_ENTRIES, playlist });

    if (res.is_null ())
        return { {}, 2 };

    searches = YTDLPTrack::get_playlist_entries (res);

#endif

#ifdef USE_SEARCH_CACHE
    // indicate if this cache is updated
    bool update_cache = searched && has_cache_id && searches_size;
    // save the result to cache
    if (update_cache)
        search_cache::set (cache_id, searches);
#endif

    if (searches.begin () == searches.end ())
        {
            return { {}, -1 };
        }

    player::MCTrack result = {};
    if (playlist == false || no_check_history)
        // play the first result according to user query
        result = searches.front ();
    else if (!no_check_history)
        {
            size_t gphs = guild_player->history.size ();

            // find entry that wasn't played before
            for (const auto &i : searches)
                {
                    auto iid = mctrack::get_id (i);
                    bool br = false;

                    // lookup in current queue
                    for (const auto &a : guild_player->queue)
                        {
                            if (mctrack::get_id (a) != iid)
                                continue;

                            br = true;
                            break;
                        }

                    if (gphs && !br)
                        {
                            // lookup in history
                            for (const auto &a : guild_player->history)
                                {
                                    if (a != iid)
                                        continue;

                                    br = true;
                                    break;
                                }
                        }

                    // current entry is in the queue or has ever been played in
                    // the last N history
                    // don't pick it
                    if (br)
                        continue;

                    result = i;
                    break;
                }

            if (result.raw.is_null ())
                {
#ifdef USE_SEARCH_CACHE
                    // invalidate cache if Id provided
                    if (has_cache_id && cached_size)
                        search_cache::remove (cache_id);
#endif
                    return { {}, 1 };
                }
        }

#ifdef USE_SEARCH_CACHE
    // save cache with key result id if update_cache is false
    if (!update_cache && !result.raw.is_null ())
        search_cache::set (result.id (), searches);
#endif

    return { result, 0 };
}

std::string
get_filename_from_result (player::MCTrack &result)
{
    const std::string sid = mctrack::get_id (result);
    const std::string st = mctrack::get_title (result);

    // ignore title for now, this is definitely problematic
    // if we want to support other track fetching method eg. radio url
    if (sid.empty () /* || st.empty()*/)
        return "";

    const std::string fullname = st + "-" + sid + ".opus";

    return std::regex_replace (fullname, std::regex ("/"), "", std::regex_constants::match_any);
}

std::pair<bool, int>
track_exist (const std::string &fname, const std::string &url, bool from_interaction, dpp::snowflake guild_id, bool no_download)
{
    if (fname.empty ())
        return { false, 2 };

    bool dling = false;
    int status = 0;

    std::ifstream test (get_music_folder_path () + fname, std::ios_base::in | std::ios_base::binary);

    if (!test.is_open ())
        {
            dling = true;
            if (from_interaction)
                status = 1;

            if (!no_download && !manager::is_waiting_file_download (fname))
                {
                    manager::download (fname, url, guild_id);
                }
        }
    else
        {
            test.close ();
            if (from_interaction)
                status = 1;
        }

    return { dling, status };
}

// fname, (unused)guild_id
using map_string_snowflake = std::map<std::string, dpp::snowflake>;
static condition_container<map_string_snowflake> waiting_file_download;

// !TODO: move this to manager namespace?
bool
run_download_thread_will_block (const player::MCTrack &result, const std::string &fname)
{
    if (guild_player_t::add_track_will_block (result))
        return true;

    auto lk = waiting_file_download.acquire ();
    return manager::is_waiting_file_download (fname);
}

void
run_download_thread (const uint32_t shard_id, const dpp::snowflake &sha_id, const bool dling, const std::string &fname, const bool arg_top,
                     const bool from_interaction, const dpp::snowflake &guild_id, const bool continued, const int64_t arg_slip,
                     const dpp::interaction_create_t &event, const player::MCTrack &result, const std::string &downloaded_response)
{
    try
        {
            dpp::snowflake user_id = from_interaction ? event.command.usr.id : sha_id;
            auto guild_player = manager::create_player (guild_id);
            if (!guild_player)
                return;

            auto *from = manager::get_client (shard_id);

            if (from_interaction)
                guild_player->set_channel (event.command.channel_id);

            if (dling)
                {
                    // waits for a while here ...
                    manager::wait_for_download (fname);
                    if (from_interaction)
                        event.edit_response (downloaded_response);
                }

            player::MCTrack t (result);
            t.filename = fname;
            t.user_id = user_id;

            guild_player->add_track (t, arg_top, guild_id, from_interaction || dling, arg_slip);

            server::ws::player::publish_queue (guild_id);

            from = manager::get_client (shard_id);
            decide_play (from, guild_id, continued);
        }
    catch (const std::exception &e)
        {
            fprintf (stderr, "[player::run_download_thread ERROR] %s\n", e.what ());
        }
}

void
run_add_track_thread (const uint32_t shard_id, const std::string &arg_query, const bool playlist, const dpp::snowflake &guild_id,
                      const std::string &cache_id, const dpp::interaction_create_t &event, const bool from_interaction,
                      const bool vcclient_cont, const dpp::snowflake &channel_id, dpp::voiceconn *v, const int64_t arg_top,
                      const int64_t arg_slip, const dpp::snowflake &sha_id, const bool continued)
{
    const bool debug = get_debug_state ();

    auto find_result = find_track (playlist, arg_query, guild_id, false, cache_id);

    switch (find_result.second)
        {
        case -1:
            if (!from_interaction)
                break;

            event.edit_response ("Can't find anything");
            return;
        case 1:
            return;
        case 0:
            break;
        case 2:
            if (!from_interaction)
                break;

            event.edit_response ("Error while searching, try again");
            return;
        default:
            fprintf (stderr, "[player::add_track WARN] Unhandled find_track return status: %d\n", find_result.second);
        }

    auto result = find_result.first;

    const std::string fname = get_filename_from_result (result);

    if (from_interaction && (vcclient_cont == false || !v))
        {
            manager::set_connecting (guild_id, channel_id);
            manager::set_waiting_vc_ready (guild_id, fname);
        }

    const auto result_url = mctrack::get_url (result);

    auto download_result = track_exist (fname, result_url, from_interaction, guild_id);
    bool dling = download_result.first;

    switch (download_result.second)
        {
        case 2:
            if (from_interaction)
                {
                    event.edit_response ("`[ERROR]` Unable to find track");
                }

            if (debug)
                fprintf (stderr, "[play::add_track ERROR] Unable to download track: `%s` `%s`\n", fname.c_str (), result_url.c_str ());

            return;
        case 1:
            if (dling)
                {
                    event.edit_response (util::response::reply_downloading_track (mctrack::get_title (result)));
                }
            else
                {
                    if (debug)
                        fprintf (stderr, "track arg_top arg_slip: '%s' %ld %ld\n", mctrack::get_title (result).c_str (), arg_top, arg_slip);

                    event.edit_response (util::response::reply_added_track (mctrack::get_title (result), arg_top ? arg_top : arg_slip));
                }
        case 0:
            break;
        default:
            fprintf (stderr, "[player::add_track WARN] Unhandled track_exist return status: %d\n", download_result.second);
        }

    manager::reconnect (manager::get_client (shard_id), guild_id);

    task::run_may_block (
        [shard_id, sha_id, dling, fname, arg_top, from_interaction, guild_id, continued, arg_slip, event, result] ()
            {
                run_download_thread (shard_id, sha_id, dling, fname, arg_top, from_interaction, guild_id, continued, arg_slip, event,
                                     result, util::response::reply_added_track (mctrack::get_title (result), arg_top ? arg_top : arg_slip));
            },
        [result, fname] () { return run_download_thread_will_block (result, fname); });
}

void
add_track (bool playlist, dpp::snowflake guild_id, std::string arg_query, int64_t arg_top, bool vcclient_cont, dpp::voiceconn *v,
           const dpp::snowflake channel_id, const dpp::snowflake sha_id, bool from_interaction, const uint32_t shard_id,
           const dpp::interaction_create_t event, bool continued, int64_t arg_slip, const std::string &cache_id)
{
    task::run_may_block (
        [shard_id, arg_query, playlist, guild_id, cache_id, event, from_interaction, vcclient_cont, channel_id, v, arg_top, arg_slip,
         sha_id, continued] ()
            {
                run_add_track_thread (shard_id, arg_query, playlist, guild_id, cache_id, event, from_interaction, vcclient_cont, channel_id,
                                      v, arg_top, arg_slip, sha_id, continued);
            },
        find_track_will_block);
}

void
decide_play (dpp::discord_client *from, const dpp::snowflake &guild_id, const bool &continued)
{
    if (!from || !get_running_state ())
        return;

    dpp::snowflake sha_id = get_sha_id ();

    std::pair<dpp::channel *, std::map<dpp::snowflake, dpp::voicestate> > vu;
    vu = get_voice_from_gid (guild_id, sha_id);

    if (!vu.first || continued || !has_listener (&vu.second))
        return;

    dpp::voiceconn *v = from->get_voice (guild_id);

    if (!v || !v->voiceclient || !v->voiceclient->is_ready ())
        return;

    if ((!v->voiceclient->is_paused () && !v->voiceclient->is_playing ()) || v->voiceclient->get_secs_remaining () < 0.05f)
        v->voiceclient->insert_marker ("s");
}

// ================================================================================

namespace manager
{
// all these map element only required the first member of the pair to be valid
// guild_id, (unused)voice_channel_id
using map_snowflake_snowflake = std::map<dpp::snowflake, dpp::snowflake>;
// guild_id, (unused)fname/voice_channel_id ?
using map_snowflake_string = std::map<dpp::snowflake, std::string>;
// guild_id
using vector_snowflake = std::vector<dpp::snowflake>;

static condition_container<map_snowflake_snowflake> connecting, disconnecting;
static condition_container<map_snowflake_string> waiting_vc_ready;
static condition_container<vector_snowflake> manually_paused;

void
reconnect (const uint32_t shard_id, const dpp::snowflake &guild_id)
{
    bool from_dc = false;
    {
        auto lk = disconnecting.unique_acquire ();
        auto a = disconnecting.container.find (guild_id);
        if (a != disconnecting.container.end ())
            {
                from_dc = true;
                task::run_once ([guild_id] () { clear_disconnecting (guild_id); }, 10);
                disconnecting.cv.wait (lk, [&guild_id] ()
                                           { return disconnecting.container.find (guild_id) == disconnecting.container.end (); });
            }
    }
    {
        auto lk = connecting.unique_acquire ();
        auto a = connecting.container.find (guild_id);
        if (a != connecting.container.end ())
            {
                {
                    using namespace std::chrono_literals;

                    // wait for 500 ms since discord will just ignore the
                    // request if it was too quick
                    if (from_dc)
                        std::this_thread::sleep_for (500ms);
                }

                dpp::discord_client *from = get_client (shard_id);

                if (from)
                    {
                        from->connect_voice (guild_id, a->second, false, SELF_DEAF, ENABLE_DAVE);
                        task::run_once ([guild_id] () { clear_connecting (guild_id); }, 10);
                        connecting.cv.wait (lk,
                                            [&guild_id] () { return connecting.container.find (guild_id) == connecting.container.end (); });
                    }
                else
                    fprintf (stderr, "[ERROR player::manager::reconnect] %u %s: Failed get_client, no connecting took place...\n", shard_id,
                             guild_id.str ().c_str ());
            }
    }
}

bool
is_disconnecting (const dpp::snowflake &guild_id)
{
    auto lk = disconnecting.acquire ();
    return disconnecting.container.find (guild_id) != disconnecting.container.end ();
}

void
set_disconnecting (const dpp::snowflake &guild_id, const dpp::snowflake &voice_channel_id)
{
    auto *g = dpp::find_guild (guild_id);
    if (!g)
        return;
    auto *client = get_client (g->shard_id);
    if (!client)
        return;
    auto *v = client->get_voice (guild_id);
    if (!v)
        // only set disconnecting if we have active voice connection
        return;

    auto lk = disconnecting.acquire ();
    disconnecting.container.insert_or_assign (guild_id, voice_channel_id);
}

static std::string
jsonobj_to_string (dpp::discord_client *dc, const nlohmann::json &json)
{
    if (dc->protocol == dpp::ws_json)
        {
            return json.dump (-1, ' ', false, nlohmann::json::error_handler_t::replace);
        }
    else
        {
            dpp::etf_parser etf;
            return etf.build (json);
        }
}

void
disconnect_voice (dpp::discord_client *dc, const dpp::snowflake &guild_id, bool force)
{
    if (!dc)
        return;

    dc->log (dpp::ll_debug, "[player::manager::disconnect_voice] Disconnecting voice, guild: " + std::to_string (guild_id));
    dc->queue_message (jsonobj_to_string (dc, nlohmann::json ({ { "op", dpp::ft_voice_state_update },
                                                                { "d",
                                                                  {
                                                                      { "guild_id", std::to_string (guild_id) },
                                                                      { "channel_id", nlohmann::json::value_t::null },
                                                                      { "self_mute", false },
                                                                      { "self_deaf", false },
                                                                  } } })),
                       false);

    std::unique_lock lock (dc->voice_mutex);
    auto vc = dc->connecting_voice_channels.find (guild_id);
    if (vc != dc->connecting_voice_channels.end ())
        dc->connecting_voice_channels.erase (vc);
}

int
wait_for_disconnecting (const dpp::snowflake &guild_id)
{

    if (!is_disconnecting (guild_id))
        return 1;

    if (get_debug_state ())
        std::cerr << "[player::manager::wait_for_disconnecting] Waiting for disconnect state: " << guild_id << '\n';

    task::run_once ([guild_id] () { clear_disconnecting (guild_id); }, 10);

    auto lk = disconnecting.unique_acquire ();
    disconnecting.cv.wait (lk, [&guild_id] () { return disconnecting.container.find (guild_id) == disconnecting.container.end (); });

    return 0;
}

void
clear_disconnecting (const dpp::snowflake &guild_id)
{
    if (get_debug_state ())
        std::cerr << "[EVENT] on_voice_state_leave: " << guild_id << '\n';

    auto lk = disconnecting.acquire ();
    auto i = disconnecting.container.find (guild_id);

    if (i != disconnecting.container.end ())
        {
            disconnecting.container.erase (i);
            disconnecting.cv.notify_all ();
        }
}

bool
is_connecting (const dpp::snowflake &guild_id)
{
    auto lk = connecting.acquire ();
    return connecting.container.find (guild_id) != connecting.container.end ();
}

void
set_connecting (const dpp::snowflake &guild_id, const dpp::snowflake &voice_channel_id)
{
    auto lk = connecting.acquire ();
    connecting.container.insert_or_assign (guild_id, voice_channel_id);
}

bool
is_waiting_vc_ready (const dpp::snowflake &guild_id)
{
    auto lk = waiting_vc_ready.acquire ();
    return waiting_vc_ready.container.find (guild_id) != waiting_vc_ready.container.end ();
}

void
set_waiting_vc_ready (const dpp::snowflake &guild_id, const std::string &second)
{
    auto *g = dpp::find_guild (guild_id);
    auto *client = g ? get_client (g->shard_id) : nullptr;
    auto *v = client ? client->get_voice (guild_id) : nullptr;
    if (v && v->voiceclient && v->voiceclient->is_ready ())
        // only set waiting_vc_ready if we dont have ready voice client
        return;

    auto lk = waiting_vc_ready.acquire ();
    waiting_vc_ready.container.insert_or_assign (guild_id, second);

    set_vc_ready_timeout (guild_id);
}

void
set_vc_ready_timeout (const dpp::snowflake &guild_id, const unsigned long &timer)
{
    task::run_once (
        [guild_id] ()
            {
                auto *cluster = get_cluster_ptr ();
                if (!cluster)
                    return;

                const int status = clear_wait_vc_ready (guild_id);

                if (status == 0)
                    return;

                bool disconnecting = false;

                auto guild_player = get_player (guild_id);

                dpp::snowflake channel_id = guild_player ? guild_player->text_channel_id : dpp::snowflake (0);

                const auto sha_id = get_sha_id ();

                std::pair<dpp::channel *, std::map<dpp::snowflake, dpp::voicestate> > vcs;

                dpp::discord_client *pc = nullptr;
                if (!guild_player || !(pc = guild_player->get_client ()))
                    goto skip_disconnecting;

                vcs = get_voice_from_gid (guild_id, sha_id);

                if (!vcs.first || !vcs.first->id)
                    goto skip_disconnecting;

                // set_disconnecting (guild_id, vcs.first->id);
                // disconnect_voice (pc, guild_id);

                fprintf (stderr,
                         "[player::manager::set_vc_ready_timeout WARN] Timeout connecting to stage/voice channel (%ld) in guild (%ld)\n",
                         (uint64_t)vcs.first->id, (uint64_t)guild_id);
                disconnecting = true;

                // this jump means there's no need to disconnect
            skip_disconnecting:
                if (!disconnecting)
                    fprintf (stderr,
                             "[player::manager::set_vc_ready_timeout WARN] Timeout connecting to stage/voice channel in guild (%ld)\n",
                             (uint64_t)guild_id);

                if (!channel_id)
                    {
                        return;
                    }

                auto server_id = guild_player->guild_id;

                bool has_send_msg_perm
                    = server_id
                      && has_permissions_from_ids (server_id, cluster->me.id, channel_id, { dpp::p_view_channel, dpp::p_send_messages });

                if (!has_send_msg_perm)
                    return;

                dpp::message m (
                    "Seems like the voice server isn't responding, try changing your voice region in the voice channel setting");

                m.set_channel_id (channel_id);

                cluster->message_create (m);
            },
        timer);
}

int
wait_for_vc_ready (const dpp::snowflake &guild_id)
{

    if (!is_waiting_vc_ready (guild_id))
        return 1;

    if (get_debug_state ())
        std::cerr << "[player::manager::wait_for_vc_ready] Waiting for ready state: " << guild_id << '\n';

    task::run_once ([guild_id] () { clear_wait_vc_ready (guild_id); }, 10);

    auto lk = waiting_vc_ready.unique_acquire ();
    waiting_vc_ready.cv.wait (lk,
                              [&guild_id] () { return waiting_vc_ready.container.find (guild_id) == waiting_vc_ready.container.end (); });

    return 0;
}

int
clear_wait_vc_ready (const dpp::snowflake &guild_id)
{
    if (get_debug_state ())
        std::cerr << "[player::manager::clear_wait_vc_ready]: " << guild_id << '\n';

    int err = clear_connecting (guild_id);

    auto lk = waiting_vc_ready.acquire ();
    auto i = waiting_vc_ready.container.find (guild_id);
    if (i != waiting_vc_ready.container.end ())
        {
            waiting_vc_ready.container.erase (i);
            waiting_vc_ready.cv.notify_all ();
            return 2;
        }

    return err;
}

int
clear_connecting (const dpp::snowflake &guild_id)
{
    if (get_debug_state ())
        std::cerr << "[player::manager::clear_connecting]: " << guild_id << '\n';

    auto lk = connecting.acquire ();
    auto i = connecting.container.find (guild_id);

    if (i != connecting.container.end ())
        {
            connecting.container.erase (i);
            connecting.cv.notify_all ();
            return 1;
        }

    return 0;
}

bool
is_manually_paused (const dpp::snowflake &guild_id)
{
    auto lk = manually_paused.acquire ();
    return vector_find (&manually_paused.container, guild_id) != manually_paused.container.end ();
}

void
set_manually_paused (const dpp::snowflake &guild_id)
{
    auto lk = manually_paused.acquire ();
    if (vector_find (&manually_paused.container, guild_id) == manually_paused.container.end ())
        {
            manually_paused.container.push_back (guild_id);
        }
}

void
clear_manually_paused (const dpp::snowflake &guild_id)
{
    auto lk = manually_paused.acquire ();
    auto i = vector_find (&manually_paused.container, guild_id);

    if (i != manually_paused.container.end ())
        {
            manually_paused.container.erase (i);
        }
}

bool
voice_ready (const dpp::snowflake &guild_id, const uint32_t shard_id, const dpp::snowflake &user_id)
{
    bool re = is_connecting (guild_id);

    if (!is_disconnecting (guild_id) && !re && !is_waiting_vc_ready (guild_id))
        return true;

    if (!re || shard_id == guild_player_t::INVALID_SHARD_ID)
        return false;

    // task::run (
    //     [shard_id, user_id, guild_id] ()
    //         {
    //             std::pair<dpp::channel *, std::map<dpp::snowflake, dpp::voicestate> > uservc;
    //
    //             uservc = get_voice_from_gid (guild_id, user_id);
    //             auto *from = player::manager::get_client (shard_id);
    //             if (!from)
    //                 return;
    //
    //             bool user_vc = uservc.first != nullptr;
    //             auto f = from->connecting_voice_channels.find (guild_id);
    //             auto c = get_voice_from_gid (guild_id, from->creator->me.id);
    //
    //             if (!c.first)
    //                 goto reset_vc;
    //
    //             if (f == from->connecting_voice_channels.end () || !f->second)
    //                 {
    //                     player::manager::set_disconnecting (guild_id, 1);
    //
    //                     player::manager::disconnect_voice (from, guild_id);
    //                 }
    //             else if (user_vc && uservc.first->id != c.first->id)
    //                 {
    //                     if (get_debug_state ())
    //                         std::cerr << "Disconnecting as it seems I just got moved to different vc and connection not updated yet: "
    //                                   << guild_id << '\n';
    //
    //                     player::manager::set_disconnecting (guild_id, f->second->channel_id);
    //
    //                     player::manager::set_connecting (guild_id, uservc.first->id);
    //
    //                     player::manager::disconnect_voice (from, guild_id);
    //                 }
    //
    //             goto reconnect;
    //
    //         reset_vc:
    //             reset_voice_channel (from, guild_id);
    //
    //             if (user_id && user_vc)
    //                 {
    //                     std::lock_guard lk (player::manager::c_m);
    //                     auto p = player::manager::connecting.find (guild_id);
    //
    //                     std::map<dpp::snowflake, dpp::voicestate> vm = {};
    //
    //                     if (p == player::manager::connecting.end ())
    //                         goto reconnect;
    //
    //                     auto gc = dpp::find_channel (p->second);
    //                     if (gc)
    //                         vm = gc->get_voice_members ();
    //
    //                     auto l = has_listener (&vm);
    //                     if (!l && p->second != uservc.first->id)
    //                         p->second = uservc.first->id;
    //                 }
    //             // goto reconnect;
    //
    //         reconnect:
    //             player::manager::reconnect (from, guild_id);
    //         });

    return true;
}

// !TODO: stop_stream should check if audio_processing loop still running!
// and return status whether flag is set or not
int
stop_stream (const dpp::snowflake &guild_id)
{
    auto guild_player = get_player (guild_id);
    if (!guild_player)
        return -1;

    guild_player->stop ();
    return 0;
}

bool
is_waiting_file_download (const std::string &file_name)
{
    return waiting_file_download.container.find (file_name) != waiting_file_download.container.end ();
}

void
wait_for_download (const std::string &file_name)
{
    auto lk = waiting_file_download.unique_acquire ();
    if (!is_waiting_file_download (file_name))
        return;

    waiting_file_download.cv.wait (
        lk, [file_name] () { return waiting_file_download.container.find (file_name) == waiting_file_download.container.end (); });
}

bool
set_info_message_as_deleted (dpp::snowflake guild_id, dpp::snowflake message_id)
{
    auto player = get_player (guild_id);
    if (!player)
        return false;

    auto pim = player->get_info_message ();
    if (pim.second != 0 || pim.first.id != message_id)
        return false;

    player->info_message["flags"] = player->info_message["flags"].get<uint16_t> () & dpp::message_flags::m_source_message_deleted;
    return true;
}

int
load_guild_current_queue (const dpp::snowflake &guild_id, const dpp::snowflake *user_id)
{
    auto player = create_player (guild_id);

    if (player->saved_queue_loaded == true)
        return 0;

    // don't do anything if no db connected
    if (database::get_conn_status () != CONNECTION_OK)
        return -1;

    player->saved_queue_loaded = true;

    std::pair<PGresult *, ExecStatusType> res = database::get_guild_current_queue (guild_id);

    std::pair<std::deque<MCTrack>, int> queue = database::get_playlist_from_PGresult (res.first);

    database::finish_res (res.first);
    res.first = nullptr;

    if (queue.second != 0)
        return queue.second;

    for (auto &t : queue.first)
        {
            if (user_id)
                t.user_id = *user_id;

            player->add_track (t);
        }

    if (queue.first.size ())
        server::ws::player::publish_queue (guild_id);

    return queue.second;
}

int
load_guild_player_config (const dpp::snowflake &guild_id)
{
    auto player = create_player (guild_id);
    if (player->saved_config_loaded == true)
        return 0;

    player->saved_config_loaded = true;

    std::pair<PGresult *, ExecStatusType> res = database::get_guild_player_config (guild_id);

    std::pair<database::player_config, int> conf = database::parse_guild_player_config_PGresult (res.first);

    database::finish_res (res.first);
    res.first = nullptr;

    if (conf.second != 0)
        return conf.second;

    player->loop_mode = conf.first.loop_mode;
    player->max_history_size = (size_t)conf.first.autoplay_threshold;
    player->auto_play = conf.first.autoplay_state;
    player->load_fx_states (conf.first.fx_states);

    return conf.second;
}

int
set_reconnect (const dpp::snowflake &guild_id, const dpp::snowflake &disconnect_channel_id, const dpp::snowflake &connect_channel_id)
{
    if (!guild_id)
        // guild_id is 0
        return -1;

    if (disconnect_channel_id)
        set_disconnecting (guild_id, disconnect_channel_id);

    if (connect_channel_id)
        {
            set_connecting (guild_id, connect_channel_id);
            set_waiting_vc_ready (guild_id);

            // success
            return 0;
        }

    // connect_channel_id is 0
    return 1;
}

int
full_reconnect (dpp::discord_client *from, const dpp::snowflake &guild_id, const dpp::snowflake &disconnect_channel_id,
                const dpp::snowflake &connect_channel_id, const bool &for_listener)
{
    const auto sha_id = get_sha_id ();

    if (for_listener)
        {
            auto m = get_voice_from_gid (guild_id, sha_id);

            if (!m.first || !has_listener (&m.second))
                return 0;
        }

    int status = set_reconnect (guild_id, disconnect_channel_id, connect_channel_id);

    disconnect_voice (from, guild_id);
    uint32_t shard_id = from->shard_id;

    reconnect (shard_id, guild_id);

    return status;
}

void
get_next_autoplay_track (const std::string &track_id, const uint32_t shard_id, const dpp::snowflake &server_id)
{
    const bool debug = get_debug_state ();

    // !TODO: limit spawning child when fetching autoplay song!
    // limit to one autoplay fetch for each guild player

    if (debug)
        fprintf (stderr, "[player::manager::handle_on_track_marker] Getting new autoplay track: %s\n", track_id.c_str ());

    const std::string query = "https://www.youtube.com/watch?v=" + track_id + "&list=RD" + track_id;

    player::add_track (true, server_id, query, 0, true, NULL, 0, get_sha_id (), false, shard_id, dpp::interaction_create_t (NULL, 0, "{}"),
                       false, 0, track_id);
}

int
set_autopause (dpp::voiceconn *v, const dpp::snowflake &guild_id, bool check_listening_user)
{
#ifndef TEST_NO_AUTOPAUSE
    if (!v || !v->voiceclient)
        return 1;

    if (is_manually_paused (guild_id))
        return -1;

    std::pair<dpp::channel *, std::map<dpp::snowflake, dpp::voicestate> > voice = { nullptr, {} };

    if (!check_listening_user)
        goto exec_pause_audio;

    voice = get_voice_from_gid (guild_id, get_sha_id ());

    // check whether there's human listening in the vc
    if (!voice.first)
        goto exec_pause_audio;

    for (const auto &l : voice.second)
        {
            // This only check user in cache,
            auto a = dpp::find_user (l.first);

            // if user not in cache then skip
            if (!a)
                continue;

            // don't count bot as listener
            if (a->is_bot ())
                continue;

            // has listening user, abort pause
            return -1;
        }

exec_pause_audio:
    v->voiceclient->pause_audio (true);
    server::ws::player::publish_pause (guild_id);
    update_info_embed (guild_id);
    return 0;
#else  // TEST_NO_AUTOPAUSE
    return 0;
#endif // TEST_NO_AUTOPAUSE
}

void
check_autopause (const dpp::snowflake &e_guild_id, const dpp::snowflake &e_voice_channel_id)
{
    const bool debug = get_debug_state ();

    bool did_manually_paused = is_manually_paused (e_guild_id);

    std::pair<dpp::channel *, dpp::voicestate *> cached = { nullptr, nullptr };
    auto guild_player = get_player (e_guild_id);
    auto *c = guild_player ? guild_player->get_client () : nullptr;
    auto *v = c ? c->get_voice (e_guild_id) : nullptr;
    auto *g = guild_player ? dpp::find_guild (e_guild_id) : nullptr;

    dpp::voicestate *vstate = nullptr;
    bool new_state_muted = false;
    bool old_state_muted = false;
    bool is_paused = false;

    int tstatus = 1;

    auto sha_id = get_sha_id ();

    if (!v)
        // no conn, skip everything
        goto end;

    // this is bizarre, voiceconn exist so continue connecting it
    if (!v->voiceclient)
        {
            // you should only call check_autopause() on bot voice state update or voice ready
            // so this makes sense to fix for connection error here
            // a cleaner way is to have dedicated function to call
            // v->connect();
            goto end;
        }

    // not modifying connection, check for server mute if not manually paused
    if (did_manually_paused)
        goto end;

    //
    // get state cache
    cached = vcs_setting_get_cache (v->channel_id);

    if (g)
        {
            auto i = g->voice_members.find (sha_id);
            if (i != g->voice_members.end ())
                vstate = &i->second;
        }

    new_state_muted = vstate ? vstate->is_mute () : false;
    old_state_muted = cached.second && cached.second->is_mute ();

    // * new state has channel
    //
    // autopause if muted and vice versa
    if (!cached.second)
        // skip if state has no voice channel
        goto end;

    is_paused = v->voiceclient->is_paused ();

    if (is_paused || !new_state_muted || old_state_muted)
        goto skip_autopause;
    // server muted, set autopause
    if (set_autopause (v, e_guild_id, false) || !debug)
        // skip if no debug
        goto end;

    std::cerr << "[player::manager::check_autopause] Paused " << e_guild_id << " as server muted\n";
    goto end;

skip_autopause:
    // else block
    if (!is_paused || new_state_muted || !old_state_muted)
        // no condition met to resume, skip resuming
        goto end;

    // server unmuted, resume
    // dispatch autoresume job
    tstatus = timer::create_resume_timer (sha_id, e_voice_channel_id, v->voiceclient.get (), 0);

    if (tstatus == 0)
        // no error, skip warn
        goto end;

    std::cerr << "[player::manager::check_autopause WARN] timer::create_resume_timer uid(" << sha_id << ") sid(" << e_guild_id << ") svcid("
              << e_voice_channel_id << ") status(" << tstatus << ")\n";

end:
    if (debug)
        {
            std::cerr << "[player::manager::check_autopause] "
                         "e_voice_channel_id("
                      << e_voice_channel_id << ") is_paused(" << is_paused << ") new_state_muted(" << new_state_muted
                      << ") old_state_muted(" << old_state_muted << ") v(" << v << ") v->voiceclient(" << (v && v->voiceclient)
                      << ") cached.first(" << cached.first << ") cached.second(" << cached.second << ")\n";
        }
}

////////////////////////////////////////

struct download_thread_params_t
{
    std::string fname;
    std::string url;
};

using download_queue_t = std::queue<download_thread_params_t>;
static exclusive_container<download_queue_t> download_q;

void
download (const std::string &fname, const std::string &url, const dpp::snowflake &guild_id)
{
    {
        auto lk = waiting_file_download.acquire ();
        waiting_file_download.container[fname] = guild_id;
    }

    auto lk = download_q.acquire ();
    download_q.container.push ({ fname, url });
}

static int
read_notif_fifo (int notif_fifo, const std::string &filepath, const std::string &url)
{
    char buf[4097];
    ssize_t cur_read = 0;
    bool has_progress = false;

    while ((cur_read = read (notif_fifo, &buf, 4096)) > 0)
        {
            buf[cur_read] = '\0';
            // log this for nice statistic or smt later
            fprintf (stderr, "%s%s", buf, buf[cur_read - 1] == '\n' ? "" : "\n");

            has_progress = true;
        }

    return has_progress ? 0 : 1;
}

static void
do_download (const std::string &fname, const std::string &url, const std::string &filepath)
{
    fprintf (stderr, "[player::manager::download] Download: \"%s\" \"%s\"\n", fname.c_str (), url.c_str ());

#ifdef MUSICAT_WITH_PYTHON
    // try using the new ytdlp::fetch() first and fallback when fail
    // it's really only able to run on one thread per call rn
    {
        nlohmann::json d;
        int ret = ytdlp::fetch (url, 1, d, filepath);
        if (ret == 0)
            return;
        else
            {
                fprintf (stderr, "[mctrack::fetch ERROR] ytdlp::fetch() Status: %d\n", ret);
                fprintf (stderr, "[mctrack::fetch ERROR] url: `%s`\n", url.c_str ());
                fprintf (stderr, "[mctrack::fetch ERROR] filepath: `%s`\n", filepath.c_str ());

                // fallback to child process
            }
    }
#endif // MUSICAT_WITH_PYTHON

    const bool debug = get_debug_state ();
    const std::string yt_dlp = get_ytdlp_exe ();

    const std::string qid = util::max_len (util::base64::encode (fname), 32);

    namespace cc = child::command;
    const std::string child_cmd = cc::create_arg_sanitize_value (cc::command_options_keys_t.id, qid)
                                  + cc::create_arg (cc::command_options_keys_t.command, cc::command_execute_commands_t.dl_music)
                                  + cc::create_arg_sanitize_value (cc::command_options_keys_t.file_path, filepath)
                                  + cc::create_arg_sanitize_value (cc::command_options_keys_t.ytdlp_query, url)
                                  + cc::create_arg_sanitize_value (cc::command_options_keys_t.ytdlp_util_exe, yt_dlp)
                                  + cc::create_arg (cc::command_options_keys_t.debug, debug ? "1" : "0");

    const std::string exit_cmd = cc::get_exit_command (qid);

    // send download command then wait until it exits
    cc::send_command (child_cmd);
    int status = child::command::wait_slave_ready (qid, 10);
    if (status != 0)
        {
            fprintf (stderr,
                     "[player::manager::download ERROR] Error downloading '%s' "
                     "to '%s' with code %d\n",
                     url.c_str (), filepath.c_str (), status);
        }
    else
        {
            const std::string notif_fifo_path = child::dl_music::get_download_music_fifo_path (qid);

            int notif_fifo = open (notif_fifo_path.c_str (), O_RDONLY);

            if (notif_fifo < 0)
                fprintf (stderr,
                         "[player::manager::download ERROR] "
                         "Failed to open notif_fifo: '%s'\n",
                         notif_fifo_path.c_str ());
            else
                {
                    status = read_notif_fifo (notif_fifo, filepath, url);
                    close (notif_fifo);
                    notif_fifo = -1;
                }

            cc::send_command (exit_cmd);
        }
}

// updates file access time without opening it
static void
update_file_access_time (const std::string &filepath)
{
    // update newly downloaded file access time
    bool utimeerr = false;
    struct stat downloaded_stat;
    struct utimbuf new_times;

    if (stat (filepath.c_str (), &downloaded_stat) == 0)
        {
            new_times.actime = time (NULL);               /* set atime to current time */
            new_times.modtime = downloaded_stat.st_mtime; /* keep mtime unchanged */
            if (utime (filepath.c_str (), &new_times) < 0)
                {
                    perror (filepath.c_str ());
                    utimeerr = true;
                }
        }
    else
        {
            perror (filepath.c_str ());
            utimeerr = true;
        }

    // if above access time update success
    if (!utimeerr)
        // tells main loop to control music cache
        set_should_check_music_cache (true);
}

static std::atomic<int> running_download = 0;
struct running_download_dec_t
{
    ~running_download_dec_t () { running_download--; }
};

void
check_download_queue ()
{
    if (running_download >= get_max_concurrent_download ())
        return;

    auto lk2 = download_q.acquire ();
    if (download_q.container.empty ())
        return;

    running_download++;
    download_thread_params_t params = download_q.container.front ();
    download_q.container.pop ();

    task::run (
        [params] ()
            {
                running_download_dec_t rdd;

                const std::string &fname = params.fname;
                const std::string &url = params.url;

                const std::string music_folder_path = get_music_folder_path ();
                util::fs::ensure_dir (music_folder_path);

                const std::string filepath = music_folder_path + fname;

                bool did_download = false;
                if (!util::fs::file_exists (filepath))
                    {
                        do_download (fname, url, filepath);
                        did_download = true;
                    }

                {
                    auto lk = waiting_file_download.acquire ();
                    waiting_file_download.container.erase (fname);

                    if (did_download)
                        update_file_access_time (filepath);
                }

                waiting_file_download.cv.notify_all ();

                // TODO: set status somewhere when needed?
            });
}

} // namespace manager

} // namespace player

// ================================================================================

namespace util
{

bool
player_has_current_track (std::shared_ptr<player::guild_player_t> guild_player)
{
    if (!guild_player || guild_player->current_track.raw.is_null () || !guild_player->queue.size ())
        return false;

    return true;
}

player::track_progress
get_track_progress (const player::MCTrack &track)
{
    int64_t duration = mctrack::get_duration (track);
    int64_t current_ms = track.current_byte ? (float)track.current_byte / player::opus_byte_per_ms : 0;
    return { current_ms, duration, 0 };
}

void
set_playback_info_track_data (nlohmann::json &data, const dpp::snowflake &guild_id, /* const */ player::MCTrack &track)
{
    // one char variable name for 100x performance improvement!!!!!
    dpp::guild_member u;
    bool member_found = false;
    try
        {
            u = dpp::find_guild_member (guild_id, track.user_id);
            member_found = true;
        }
    catch (...)
        {
        }

    dpp::user *uc = dpp::find_user (track.user_id);
    std::string track_username;
    if (member_found)
        {
            std::string nick = u.get_nickname ();
            if (!nick.empty ())
                track_username = nick;
        }

    if (track_username.empty ())
        {
            if (uc)
                track_username = uc->username;
            else
                track_username = "";
        }

    std::string track_user_avatar = member_found ? u.get_avatar_url (4096) : "";
    if (track_user_avatar.empty () && uc)
        track_user_avatar = uc->get_avatar_url (4096);

    player::track_progress prog = get_track_progress (track);

    data["username"] = track_username;
    data["avatar"] = track_user_avatar;
    // !TODO: this call should const able!
    data["thumbnail"] = mctrack::get_thumbnail (track);
    data["desc"] = mctrack::get_description (track);
    data["title"] = mctrack::get_title (track);
    data["url"] = mctrack::get_url (track);
    data["progress"] = prog.current_ms;
    data["duration"] = prog.duration;

    // !TODO: remove this when fully using ytdlp to support non-yt
    // tracks
    bool tinfo = !track.info.raw.is_null ();
    if (tinfo)
        data["average_bitrate"] = track.info.average_bitrate ();
}

static void
set_guild_player_data (nlohmann::json &data, const dpp::snowflake &guild_id)
{
    auto guild_player = player::manager::get_player (guild_id);
    if (!guild_player)
        return;

    data["loop_mode"] = guild_player->loop_mode;
    data["auto_play"] = guild_player->auto_play;
    data["repeat"] = guild_player->current_track.repeat;
    data["active_fx"] = guild_player->fx_get_active_count ();

    bool playing_set = false;
    auto *pc = guild_player->get_client ();
    if (pc)
        {
            auto con = pc->get_voice (guild_id);
            if (con && con->voiceclient)
                {
                    data["paused"] = con->voiceclient->is_paused ();
                    data["playing"] = true;
                    playing_set = true;
                }
        }

    if (!playing_set)
        {
            data["paused"] = false;
            data["playing"] = false;
        }

    // current_track only valid when processing_audio
    if (!guild_player->processing_audio)
        return;

    player::MCTrack &track = guild_player->current_track;
    set_playback_info_track_data (data, guild_id, track);
}

nlohmann::json
get_playback_info_json (const dpp::snowflake &guild_id)
{
    try
        {
            // this proly shouldnt be here
            // new dedicated event for guild member info?
            auto h_r = get_user_highest_role (guild_id, get_sha_id ());
            uint32_t color = h_r ? h_r->colour : 0;

            auto data = nlohmann::json::object ({ {
                "color_hint",
                color,
            } });

            set_guild_player_data (data, guild_id);

            return data;
        }
    catch (const dpp::exception &e)
        {
            fprintf (stderr, "[util::get_playback_info_json dpp::exception]: %s\n", e.what ());
        }
    catch (const std::logic_error &e)
        {
            fprintf (stderr, "[util::get_playback_info_json std::logic_error]: %s\n", e.what ());
        }

    return nullptr;
}

} // namespace util

} // namespace musicat
