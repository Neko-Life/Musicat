#include "musicat/YTDLPTrack.h"
#include "musicat/db.h"
#include "musicat/mctrack.h"
#include "musicat/musicat.h"
#include "musicat/player.h"
#include "musicat/thread_manager.h"
#include "musicat/util.h"
#include "musicat/util_response.h"
#include <dirent.h>
#include <libpq-fe.h>
#include <regex>
#include <sys/stat.h>

/* #define USE_SEARCH_CACHE */

namespace musicat
{
namespace player
{
using string = std::string;

enum gat_cache_completeness_e
{
    GATCC_NONE = 0,
    GATCC_ZERO_AMOUNT = 1,
    GATCC_HAS_STAT = 1 << 1,
};

std::mutex gat_m;
std::vector<gat_t> gat_ret_cache;
time_t gat_last_uc = 0;
int gat_cache_completeness = 0;

// returns true if gat_cache_completeness is missing flag
// and required by rc
bool
gatcc_rnot (int rc, int flag)
{
    return (rc & flag) == flag && (gat_cache_completeness & flag) != flag;
}

bool
gatcc_is_complete ()
{
    constexpr const int complete_f = GATCC_ZERO_AMOUNT | GATCC_HAS_STAT;
    return (gat_cache_completeness & complete_f) == complete_f;
}

bool
gatcc_rnot_zero_amount (int rc)
{
    return gatcc_rnot (rc, GATCC_ZERO_AMOUNT);
}

bool
gatcc_rnot_has_stat (int rc)
{
    return gatcc_rnot (rc, GATCC_HAS_STAT);
}

void
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
        {
            required_completeness |= GATCC_ZERO_AMOUNT;
        }

    if (with_stat)
        {
            required_completeness |= GATCC_HAS_STAT;
        }

    time_t cur_t = time (NULL);

    // cache last updated was less than 11 second ago
    if ((cur_t - gat_last_uc) < 11)
        {
            // check for cache completeness
            bool is_sufficient = true;

            if (!gatcc_is_complete ())
                is_sufficient
                    = !gatcc_rnot_zero_amount (required_completeness)
                      && !gatcc_rnot_has_stat (required_completeness);

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
    string s;
    // full file path
    string fpath;
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

            s = string (file->d_name);

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
                            perror ("[Manager::get_available_"
                                    "tracks] stat");

                            fprintf (stderr, "^^^ Failed stating '%s'\n",
                                     fpath.c_str ());

                            goto sk_stat;
                        }

                    siz = st.st_size;
                    last_access = st.st_atime;
                }
        sk_stat:
            ret.push_back (
                { s.substr (0, opus_ext_idx), s, fpath, siz, last_access });

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

    fprintf (stderr,
             "[main::loop] Current cached music: %ld "
             "files (%ld bytes)\n",
             ats.size (), cur_cache_size);

    if (cur_cache_size <= size_limit)
        // current cached music size does not go over limit
        return;

    // currect cached music is over the limit defined
    // in conf

    fprintf (stderr,
             "[main::loop] Current cached music "
             "size goes over %ld bytes limit, "
             "cleaning old music...\n",
             size_limit);

    // sort by oldest first
    std::sort (ats.begin (), ats.end (), [] (const gat_t &a, const gat_t &b) {
        return a.last_access < b.last_access;
    });

    size_t prev_cache_size = cur_cache_size;
    size_t rc = 0;

    // delete everything until size is less
    // than limit
    for (const gat_t &g : ats)
        {
            if (cur_cache_size < size_limit)
                break;

            fprintf (stderr,
                     "[main::loop] "
                     "Unlinking '%s'\n",
                     g.fullpath.c_str ());

            if (unlink (g.fullpath.c_str ()) == 0)
                {
                    cur_cache_size -= g.size;
                    rc++;
                    continue;
                }

            perror ("[main::loop] unlink");
            fprintf (stderr,
                     "^^^ Failed unlink "
                     "'%s'\n",
                     g.fullpath.c_str ());
        }

    fprintf (stderr,
             "[main::loop] Cleaned up %ld "
             "music files and %ld bytes worth "
             "of storage space\n",
             rc, prev_cache_size - cur_cache_size);

    invalidate_track_list_cache ();
}

// ================================================================================

std::mutex tfpc_m;
std::map<std::string, int> track_failed_playback_counts;

int
get_track_failed_playback_count (const std::string &filename)
{
    if (filename.empty ())
        {
            if (get_debug_state ())
                {
                    fprintf (stderr, "[player::get_track_failed_playback_"
                                     "count ERROR] track.filename is empty\n");
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
                    fprintf (stderr,
                             "[player::set_track_failed_playback_"
                             "count ERROR] `track.filename` is empty\n");
                }

            return -1;
        }

    if (c < 0)
        {
            if (debug)
                {
                    fprintf (stderr,
                             "[player::set_track_failed_playback_"
                             "count ERROR] `c` can't be less than 0\n");
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

std::pair<player::MCTrack, int>
find_track (const bool playlist, const std::string &arg_query,
            player::player_manager_ptr_t player_manager,
            const dpp::snowflake guild_id, const bool no_check_history,
            const std::string &cache_id)
{
    std::string trimmed_query = util::trim_str (arg_query);

#ifdef USE_SEARCH_CACHE
    bool has_cache_id = !cache_id.empty ();
#else
    // bool has_cache_id = false;
#endif

    std::shared_ptr<player::Player> guild_player = NULL;

    // i wonder what was this for... i do the one who wrote this but i forgot
    if (playlist && !no_check_history)
        {
            guild_player = player_manager->get_player (guild_id);

            if (!guild_player)
                return { {}, 1 };

            // if there's no track return without searching first?
            std::lock_guard<std::mutex> lk (guild_player->t_mutex);
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
                    searches
                        = playlist
                              ? (playlist_result
                                 = yt_search::get_playlist (trimmed_query))
                                    .entries ()

                              : (search_result
                                 = yt_search::search (trimmed_query))
                                    .trackResults ();

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
                    std::cerr << "[player::find_track ERROR] " << guild_id
                              << ':' << e.what () << '\n';

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
    nlohmann::json res = mctrack::fetch (
        { trimmed_query, YDLP_DEFAULT_MAX_ENTRIES, playlist });

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
    const string sid = mctrack::get_id (result);
    const string st = mctrack::get_title (result);

    // ignore title for now, this is definitely problematic
    // if we want to support other track fetching method eg. radio url
    if (sid.empty () /* || st.empty()*/)
        return "";

    const string fullname = st + "-" + sid + ".opus";

    return std::regex_replace (fullname, std::regex ("/"), "",
                               std::regex_constants::match_any);
}

std::pair<bool, int>
track_exist (const std::string &fname, const std::string &url,
             player::player_manager_ptr_t player_manager,
             bool from_interaction, dpp::snowflake guild_id, bool no_download)
{
    if (fname.empty ())
        return { false, 2 };

    bool dling = false;
    int status = 0;

    std::ifstream test (get_music_folder_path () + fname,
                        std::ios_base::in | std::ios_base::binary);

    if (!test.is_open ())
        {
            dling = true;
            if (from_interaction)
                status = 1;

            if (!no_download
                && player_manager->waiting_file_download.find (fname)
                       == player_manager->waiting_file_download.end ())
                {
                    player_manager->download (fname, url, guild_id);
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

void
add_track (bool playlist, dpp::snowflake guild_id, std::string arg_query,
           int64_t arg_top, bool vcclient_cont, dpp::voiceconn *v,
           const dpp::snowflake channel_id, const dpp::snowflake sha_id,
           bool from_interaction, dpp::discord_client *from,
           const dpp::interaction_create_t event, bool continued,
           int64_t arg_slip, const std::string &cache_id)
{
    std::thread rt ([=] () {
        thread_manager::DoneSetter tmds;

        auto player_manager = get_player_manager_ptr ();
        if (!player_manager)
            {
                fprintf (stderr,
                         "[player::add_track WARN] Can't add track with "
                         "query, no manager: %s\n",
                         arg_query.c_str ());

                return;
            }

        const bool debug = get_debug_state ();

        auto find_result = find_track (playlist, arg_query, player_manager,
                                       guild_id, false, cache_id);

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
                fprintf (stderr,
                         "[player::add_track WARN] Unhandled find_track "
                         "return status: %d\n",
                         find_result.second);
            }

        auto result = find_result.first;

        const std::string fname = get_filename_from_result (result);

        if (from_interaction && (vcclient_cont == false || !v))
            {
                player_manager->set_connecting (guild_id, channel_id);
                player_manager->set_waiting_vc_ready (guild_id, fname);
            }

        const auto result_url = mctrack::get_url (result);

        auto download_result = track_exist (fname, result_url, player_manager,
                                            from_interaction, guild_id);
        bool dling = download_result.first;

        switch (download_result.second)
            {
            case 2:
                if (from_interaction)
                    {
                        event.edit_response ("`[ERROR]` Unable to find track");
                    }

                if (debug)
                    fprintf (
                        stderr,
                        "[play::add_track ERROR] Unable to download track: "
                        "`%s` `%s`\n",
                        fname.c_str (), result_url.c_str ());

                return;
            case 1:
                if (dling)
                    {
                        event.edit_response (
                            util::response::reply_downloading_track (
                                mctrack::get_title (result)));
                    }
                else
                    {
                        if (debug)
                            fprintf (stderr,
                                     "track arg_top arg_slip: '%s' %ld %ld\n",
                                     mctrack::get_title (result).c_str (),
                                     arg_top, arg_slip);

                        event.edit_response (
                            util::response::reply_added_track (
                                mctrack::get_title (result),
                                arg_top ? arg_top : arg_slip));
                    }
            case 0:
                break;
            default:
                fprintf (stderr,
                         "[player::add_track WARN] Unhandled track_exist "
                         "return status: %d\n",
                         download_result.second);
            }

        std::thread pjt ([player_manager, from, guild_id] () {
            thread_manager::DoneSetter tmds;
            player_manager->reconnect (from, guild_id);
        });

        thread_manager::dispatch (pjt);

        std::thread dlt (
            [player_manager, sha_id, dling, fname, arg_top, from_interaction,
             guild_id, from, continued, arg_slip,
             event] (player::MCTrack result) {
                thread_manager::DoneSetter tmds;

                dpp::snowflake user_id
                    = from_interaction ? event.command.usr.id : sha_id;
                auto guild_player = player_manager->create_player (guild_id);

                const dpp::snowflake channel_id
                    = from_interaction ? event.command.channel_id
                                       : guild_player->channel_id;

                if (dling)
                    {
                        player_manager->wait_for_download (fname);
                        if (from_interaction)
                            event.edit_response (
                                util::response::reply_added_track (
                                    mctrack::get_title (result),
                                    arg_top ? arg_top : arg_slip));
                    }
                if (from)
                    guild_player->from = from;

                player::MCTrack t (result);
                t.filename = fname;
                t.user_id = user_id;

                guild_player->add_track (t, arg_top ? true : false, guild_id,
                                         from_interaction || dling, arg_slip);

                if (from_interaction)
                    guild_player->set_channel (channel_id);

                decide_play (from, guild_id, continued);
            },
            result);

        thread_manager::dispatch (dlt);
    });

    thread_manager::dispatch (rt);
}

void
decide_play (dpp::discord_client *from, const dpp::snowflake &guild_id,
             const bool &continued)
{
    if (!from)
        return;

    dpp::snowflake sha_id = get_sha_id ();

    std::pair<dpp::channel *, std::map<dpp::snowflake, dpp::voicestate> > vu;
    vu = get_voice_from_gid (guild_id, sha_id);

    if (!vu.first || continued || !has_listener (&vu.second))
        return;

    dpp::voiceconn *v = from->get_voice (guild_id);

    if (!v || !v->voiceclient || !v->voiceclient->is_ready ())
        return;

    if ((!v->voiceclient->is_paused () && !v->voiceclient->is_playing ())
        || v->voiceclient->get_secs_remaining () < 0.05f)
        v->voiceclient->insert_marker ("s");
}

// ================================================================================

bool
Manager::is_disconnecting (const dpp::snowflake &guild_id)
{
    std::lock_guard lk1 (this->dc_m);
    return this->disconnecting.find (guild_id) != this->disconnecting.end ();
}

void
Manager::set_disconnecting (const dpp::snowflake &guild_id,
                            const dpp::snowflake &voice_channel_id)
{
    std::lock_guard lk (this->dc_m);

    this->disconnecting.insert_or_assign (guild_id, voice_channel_id);
}

void
Manager::clear_disconnecting (const dpp::snowflake &guild_id)
{
    if (get_debug_state ())
        std::cerr << "[EVENT] on_voice_state_leave: " << guild_id << '\n';

    std::lock_guard lk (this->dc_m);

    auto i = this->disconnecting.find (guild_id);

    if (i != this->disconnecting.end ())
        {
            this->disconnecting.erase (i);
            this->dl_cv.notify_all ();
        }
}

bool
Manager::is_connecting (const dpp::snowflake &guild_id)
{
    std::lock_guard lk2 (this->c_m);
    return this->connecting.find (guild_id) != this->connecting.end ();
}

void
Manager::set_connecting (const dpp::snowflake &guild_id,
                         const dpp::snowflake &voice_channel_id)
{
    std::lock_guard lk (this->c_m);

    this->connecting.insert_or_assign (guild_id, voice_channel_id);
}

bool
Manager::is_waiting_vc_ready (const dpp::snowflake &guild_id)
{
    std::lock_guard lk3 (this->wd_m);
    return this->waiting_vc_ready.find (guild_id)
           != this->waiting_vc_ready.end ();
}

void
Manager::set_waiting_vc_ready (const dpp::snowflake &guild_id,
                               const std::string &second)
{
    std::lock_guard lk2 (this->wd_m);

    this->waiting_vc_ready.insert_or_assign (guild_id, second);

    this->set_vc_ready_timeout (guild_id);
}

void
Manager::set_vc_ready_timeout (const dpp::snowflake &guild_id,
                               const unsigned long &timer)
{
    std::thread t ([this, guild_id, timer] () {
        thread_manager::DoneSetter tmds;

        std::this_thread::sleep_for (std::chrono::milliseconds (timer));

        const int status = this->clear_wait_vc_ready (guild_id);

        if (status == 0)
            return;

        fprintf (stderr, "[Manager::set_vc_ready_timeout "
                         "WARN] Connection timeout\n");

        auto player_manager = get_player_manager_ptr ();

        auto guild_player
            = player_manager ? player_manager->get_player (guild_id) : nullptr;

        dpp::snowflake channel_id
            = guild_player ? guild_player->channel_id : dpp::snowflake (0);

        const auto sha_id = get_sha_id ();

        std::pair<dpp::channel *, std::map<dpp::snowflake, dpp::voicestate> >
            vcs;

        if (!player_manager || !guild_player || !guild_player->from)
            goto skip_disconnecting;

        vcs = get_voice_from_gid (guild_id, sha_id);

        if (!vcs.first || !vcs.first->id)
            goto skip_disconnecting;

        player_manager->set_disconnecting (guild_id, vcs.first->id);

        guild_player->from->disconnect_voice (guild_id);

        // this jump means there's no need to disconnect
    skip_disconnecting:

        if (!channel_id)
            {
                return;
            }

        auto server_id = guild_player->guild_id;

        bool has_send_msg_perm
            = server_id
              && has_permissions_from_ids (
                  server_id, this->cluster->me.id, channel_id,
                  { dpp::p_view_channel, dpp::p_send_messages });

        if (!has_send_msg_perm)
            return;

        dpp::message m ("Seems like the voice "
                        "server isn't "
                        "responding, try "
                        "changing your voice "
                        "region in the voice "
                        "channel setting");

        m.set_channel_id (channel_id);

        this->cluster->message_create (m);
    });

    thread_manager::dispatch (t);
}

void
Manager::wait_for_vc_ready (const dpp::snowflake &guild_id)
{

    if (!is_waiting_vc_ready (guild_id))
        return;

    if (get_debug_state ())
        std::cerr << "[Manager::wait_for_vc_ready] Waiting for ready state: "
                  << guild_id << '\n';

    std::unique_lock lk (this->wd_m);
    this->dl_cv.wait (lk, [this, &guild_id] () {
        return this->waiting_vc_ready.find (guild_id)
               == this->waiting_vc_ready.end ();
    });
}

int
Manager::clear_wait_vc_ready (const dpp::snowflake &guild_id)
{
    if (get_debug_state ())
        std::cerr << "[Manager::clear_wait_vc_ready]: " << guild_id << '\n';

    int err = this->clear_connecting (guild_id);

    std::lock_guard lk (this->wd_m);

    auto i = this->waiting_vc_ready.find (guild_id);

    if (i != this->waiting_vc_ready.end ())
        {
            this->waiting_vc_ready.erase (i);
            this->dl_cv.notify_all ();
            return 2;
        }

    return err;
}

int
Manager::clear_connecting (const dpp::snowflake &guild_id)
{
    if (get_debug_state ())
        std::cerr << "[Manager::clear_connecting]: " << guild_id << '\n';

    std::lock_guard lk (this->c_m);

    auto i = this->connecting.find (guild_id);

    if (i != this->connecting.end ())
        {
            this->connecting.erase (i);
            this->dl_cv.notify_all ();
            return 1;
        }

    return 0;
}

bool
Manager::is_manually_paused (const dpp::snowflake &guild_id)
{
    std::lock_guard lk (this->mp_m);

    return vector_find (&this->manually_paused, guild_id)
           != this->manually_paused.end ();
}

void
Manager::set_manually_paused (const dpp::snowflake &guild_id)
{
    std::lock_guard lk (this->mp_m);

    if (vector_find (&this->manually_paused, guild_id)
        == this->manually_paused.end ())
        {
            this->manually_paused.push_back (guild_id);
        }
}

void
Manager::clear_manually_paused (const dpp::snowflake &guild_id)
{
    std::lock_guard lk (this->mp_m);

    auto i = vector_find (&this->manually_paused, guild_id);

    if (i != this->manually_paused.end ())
        {
            this->manually_paused.erase (i);
        }
}

bool
Manager::voice_ready (const dpp::snowflake &guild_id,
                      dpp::discord_client *from, const dpp::snowflake &user_id)
{
    bool re = is_connecting (guild_id);

    if (!is_disconnecting (guild_id) && !re && !is_waiting_vc_ready (guild_id))
        return true;

    if (!re || !from)
        return false;

    std::thread t (
        [this, user_id, guild_id] (dpp::discord_client *from) {
            thread_manager::DoneSetter tmds;

            std::pair<dpp::channel *,
                      std::map<dpp::snowflake, dpp::voicestate> >
                uservc;

            uservc = get_voice_from_gid (guild_id, user_id);

            bool user_vc = uservc.first != nullptr;
            auto f = from->connecting_voice_channels.find (guild_id);
            auto c = get_voice_from_gid (guild_id, from->creator->me.id);

            if (!c.first)
                goto reset_vc;

            if (f == from->connecting_voice_channels.end () || !f->second)
                {
                    this->set_disconnecting (guild_id, 1);

                    from->disconnect_voice (guild_id);
                }
            else if (user_vc && uservc.first->id != c.first->id)
                {
                    if (get_debug_state ())
                        std::cerr << "Disconnecting as it "
                                     "seems I just got moved "
                                     "to different vc and "
                                     "connection not updated "
                                     "yet: "
                                  << guild_id << '\n';

                    this->set_disconnecting (guild_id, f->second->channel_id);

                    this->set_connecting (guild_id, uservc.first->id);

                    from->disconnect_voice (guild_id);
                }

            goto reconnect;

        reset_vc:
            reset_voice_channel (from, guild_id);

            if (user_id && user_vc)
                {
                    std::lock_guard lk (this->c_m);
                    auto p = this->connecting.find (guild_id);

                    std::map<dpp::snowflake, dpp::voicestate> vm = {};

                    if (p == this->connecting.end ())
                        goto reconnect;

                    auto gc = dpp::find_channel (p->second);
                    if (gc)
                        vm = gc->get_voice_members ();

                    auto l = has_listener (&vm);
                    if (!l && p->second != uservc.first->id)
                        p->second = uservc.first->id;
                }
            // goto reconnect;

        reconnect:
            this->reconnect (from, guild_id);
        },
        from);

    thread_manager::dispatch (t);

    return true;
}

// !TODO: stop_stream should check if audio_processing loop still running!
// and return status whether flag is set or not
int
Manager::stop_stream (const dpp::snowflake &guild_id)
{
    auto vs = get_voice_from_gid (guild_id, get_sha_id ());
    if (!vs.first)
        return -1;

    auto guild_player = this->get_player (guild_id);

    if (!guild_player || !guild_player->processing_audio)
        return -1;

    guild_player->current_track.stopping = true;
    return set_stream_stopping (guild_id);
}

bool
Manager::is_waiting_file_download (const string &file_name)
{
    return this->waiting_file_download.find (file_name)
           != this->waiting_file_download.end ();
}

void
Manager::wait_for_download (const string &file_name)
{
    std::unique_lock lk (this->dl_m);

    if (!this->is_waiting_file_download (file_name))
        return;

    this->dl_cv.wait (lk, [this, file_name] () {
        return this->waiting_file_download.find (file_name)
               == this->waiting_file_download.end ();
    });
}

bool
Manager::set_info_message_as_deleted (dpp::snowflake id)
{
    std::lock_guard lk (this->imc_m);
    auto m = this->info_messages_cache.find (id);
    if (m != this->info_messages_cache.end ())
        {
            if (!m->second->is_source_message_deleted ())
                {
                    m->second->set_flags (m->second->flags
                                          | dpp::m_source_message_deleted);
                    return true;
                }
        }
    return false;
}

void
Manager::set_ignore_marker (const dpp::snowflake &guild_id)
{
    std::lock_guard lk (this->im_m);
    auto l = vector_find (&this->ignore_marker, guild_id);
    if (l == this->ignore_marker.end ())
        {
            this->ignore_marker.push_back (guild_id);
        }
}

void
Manager::remove_ignore_marker (const dpp::snowflake &guild_id)
{
    std::lock_guard lk (this->im_m);

    auto i = vector_find (&this->ignore_marker, guild_id);

    if (i != this->ignore_marker.end ())
        {
            this->ignore_marker.erase (i);
        }
}

bool
Manager::has_ignore_marker (const dpp::snowflake &guild_id)
{
    std::lock_guard lk (this->im_m);
    auto l = vector_find (&this->ignore_marker, guild_id);
    if (l != this->ignore_marker.end ())
        return true;
    else
        return false;
}

int
Manager::load_guild_current_queue (const dpp::snowflake &guild_id,
                                   const dpp::snowflake *user_id)
{
    auto player = this->create_player (guild_id);

    if (player->saved_queue_loaded == true)
        return 0;

    // don't do anything if no db connected
    if (database::get_conn_status () != CONNECTION_OK)
        return -1;

    player->saved_queue_loaded = true;

    std::pair<PGresult *, ExecStatusType> res
        = database::get_guild_current_queue (guild_id);

    std::pair<std::deque<MCTrack>, int> queue
        = database::get_playlist_from_PGresult (res.first);

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

    return queue.second;
}

int
Manager::load_guild_player_config (const dpp::snowflake &guild_id)
{
    auto player = this->create_player (guild_id);
    if (player->saved_config_loaded == true)
        return 0;

    player->saved_config_loaded = true;

    std::pair<PGresult *, ExecStatusType> res
        = database::get_guild_player_config (guild_id);

    std::pair<database::player_config, int> conf
        = database::parse_guild_player_config_PGresult (res.first);

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
Manager::set_reconnect (const dpp::snowflake &guild_id,
                        const dpp::snowflake &disconnect_channel_id,
                        const dpp::snowflake &connect_channel_id)
{
    if (!guild_id)
        // guild_id is 0
        return -1;

    if (disconnect_channel_id)
        this->set_disconnecting (guild_id, disconnect_channel_id);

    if (connect_channel_id)
        {
            this->set_connecting (guild_id, connect_channel_id);
            this->set_waiting_vc_ready (guild_id);

            // success
            return 0;
        }

    // connect_channel_id is 0
    return 1;
}

int
Manager::full_reconnect (dpp::discord_client *from,
                         const dpp::snowflake &guild_id,
                         const dpp::snowflake &disconnect_channel_id,
                         const dpp::snowflake &connect_channel_id,
                         const bool &for_listener)
{
    const auto sha_id = get_sha_id ();

    if (for_listener)
        {
            auto m = get_voice_from_gid (guild_id, sha_id);

            if (!m.first || !has_listener (&m.second))
                return 0;
        }

    int status = this->set_reconnect (guild_id, disconnect_channel_id,
                                      connect_channel_id);

    from->disconnect_voice (guild_id);

    std::thread pjt ([this, from, guild_id] () {
        thread_manager::DoneSetter tmds;

        this->reconnect (from, guild_id);
    });

    thread_manager::dispatch (pjt);

    return status;
}

void
Manager::get_next_autoplay_track (const string &track_id,
                                  dpp::discord_client *from,
                                  const dpp::snowflake &server_id)
{
    const bool debug = get_debug_state ();

    // !TODO: limit spawning child when fetching autoplay song!
    // limit to one autoplay fetch for each guild player

    if (debug)
        fprintf (stderr,
                 "[Manager::handle_on_track_marker] Getting new autoplay "
                 "track: %s\n",
                 track_id.c_str ());

    const string query = "https://www.youtube.com/watch?v=" + track_id
                         + "&list=RD" + track_id;

    player::add_track (true, server_id, query, 0, true, NULL, 0, get_sha_id (),
                       false, from, dpp::interaction_create_t (NULL, "{}"),
                       false, 0, track_id);
}

int
Manager::set_autopause (dpp::voiceconn *v, const dpp::snowflake &guild_id,
                        bool check_listening_user)
{
    if (!v || !v->voiceclient)
        return 1;

    if (this->is_manually_paused (guild_id))
        return -1;

    std::pair<dpp::channel *, std::map<dpp::snowflake, dpp::voicestate> > voice
        = { nullptr, {} };

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
    this->update_info_embed (guild_id);
    return 0;
}

} // player
} // musicat
