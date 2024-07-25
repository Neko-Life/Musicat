#include "musicat/player_manager_timer.h"
#include "musicat/musicat.h"
#include "musicat/util.h"

namespace musicat::player::timer
{
/* static std::vector<connect_timer_t> ct_v; */
static std::vector<track_marker_rm_timer_t> tmrmt_v;
static std::vector<resume_timer_t> rt_v;
static std::vector<failed_playback_reset_timer_t> fprt_v;

/* static std::mutex ct_m; */
static std::mutex tmrmt_m;
static std::mutex rt_m;
static std::mutex fprt_m;

void
check_connect_timers ()
{
    // !TODO
}

static bool
is_tmrmt_duplicate (const track_marker_rm_timer_t &tmrmt)
{
    auto i = tmrmt_v.begin ();

    while (i != tmrmt_v.end ())
        {
            bool met = i->meta != tmrmt.meta;
            bool sids = i->vc->server_id != tmrmt.vc->server_id;
            bool cids = i->vc->channel_id != tmrmt.vc->channel_id;

            if (met || sids || cids)
                {
                    i++;
                    continue;
                }

            return true;
        }

    return false;
}

static bool
is_rt_duplicate (const resume_timer_t &rt)
{
    auto i = rt_v.begin ();

    while (i != rt_v.end ())
        {
            bool sids = i->vc->server_id != rt.vc->server_id;
            bool svcids = i->svcid != rt.svcid;
            bool cids = i->vc->channel_id != rt.vc->channel_id;

            if (sids || svcids || cids)
                {
                    i++;
                    continue;
                }

            return true;
        }

    return false;
}

int
create_track_marker_rm_timer (const std::string &meta,
                              dpp::discord_voice_client *vc)
{
    if (!vc)
        return 1;

    std::lock_guard lk (tmrmt_m);

    track_marker_rm_timer_t tmrmt = { util::get_current_ts (), meta, vc };

    if (get_debug_state ())
        {
            std::cerr << "[player::timer::create_track_marker_rm_timer] "
                         "Creating track_marker_rm_timer_t: ts("
                      << tmrmt.ts << ") vcsid(" << tmrmt.vc->server_id
                      << ") vcvcid(" << tmrmt.vc->channel_id << ")\n";
        }

    bool no = is_tmrmt_duplicate (tmrmt);
    if (no)
        return 2;

    tmrmt_v.push_back (tmrmt);
    return 0;
}

void
check_track_marker_rm_timers ()
{
    bool exiting = !get_running_state ();
    bool debug = get_debug_state ();
    auto player_manager = get_player_manager_ptr ();

    std::lock_guard lk (tmrmt_m);

    auto now = util::get_current_ts ();
    auto i = tmrmt_v.begin ();
    while (i != tmrmt_v.end ())
        {
            dpp::discord_voice_client *vc = NULL;
            std::shared_ptr<Player> player = NULL;

            long long rm_threshold;
            constexpr long long ms5k = util::ms_to_picos (5000);
            constexpr long long ms20k = util::ms_to_picos (20000);
            bool removed = false;

            if (exiting || !player_manager)
                goto erase;

            // begin
            vc = i->vc;

            if (!vc)
                goto erase;

            if (vc->terminating)
                goto remove_erase;

            player = player_manager->get_player (vc->server_id);

            if (!player)
                goto remove_erase;

            rm_threshold = player->saved_queue_loaded ? ms5k : ms20k;

            if (((now - i->ts) < rm_threshold) && !vc->is_playing ()
                && !vc->is_paused ())
                {
                    i++;
                    continue;
                }

        remove_erase:
            player_manager->remove_ignore_marker (vc->server_id);
            removed = true;

        erase:
            i = tmrmt_v.erase (i);

            if (!debug)
                continue;

            fprintf (stderr,
                     "[player::timer::check_track_marker_rm_timers] "
                     "Erased ignore marker for meta '%s': "
                     "removed(%d) server_id(",
                     i->meta.c_str (), removed);

            std::cerr << vc->server_id;

            fprintf (stderr, ")\n");
        }
}

int
create_resume_timer (const dpp::snowflake &user_id,
                     const dpp::snowflake &user_voice_channel_id,
                     dpp::discord_voice_client *vc, long long min_delay)
{
    if (!vc)
        return 1;

    std::lock_guard lk (rt_m);

    resume_timer_t rt = { util::get_current_ts (), user_id,
                          user_voice_channel_id, vc, min_delay };

    if (get_debug_state ())
        {
            std::cerr << "[player::timer::create_resume_timer] "
                         "Creating resume_timer_t: ts("
                      << rt.ts << ") uid(" << rt.uid << ") svcid(" << rt.svcid
                      << ") vcsid(" << rt.vc->server_id << ") vcvcid("
                      << rt.vc->channel_id << ")\n";
        }

    bool no = is_rt_duplicate (rt);
    if (no)
        return 2;

    rt_v.push_back (rt);
    return 0;
}

void
check_resume_timers ()
{
    bool exiting = !get_running_state ();
    bool debug = get_debug_state ();
    auto player_manager = get_player_manager_ptr ();

    std::lock_guard lk (rt_m);

    auto now = util::get_current_ts ();
    auto i = rt_v.begin ();
    while (i != rt_v.end ())
        {
            if (exiting || !player_manager)
                {
                    i = rt_v.erase (i);
                    continue;
                }

            auto diff = (now - i->ts);
            // less than 1.5 second passed
            if (diff < i->min_delay)
                {
                    i++;
                    continue;
                }

            auto vc = i->vc;

            if (!vc || vc->terminating)
                {
                    i = rt_v.erase (i);
                    continue;
                }

            auto vgid = get_voice_from_gid (vc->server_id, i->uid);

            if (!vgid.first || vgid.first->id != i->svcid)
                {
                    i = rt_v.erase (i);
                    continue;
                }

            if (debug)
                {
                    std::cerr << "[player::timer::check_resume_timers] "
                                 "Unpausing resume_timer_t: now("
                              << now << ") ts(" << i->ts << ") diff(" << diff
                              << ") uid(" << i->uid << ") svcid(" << i->svcid
                              << ") vcsid(" << i->vc->server_id << ") vcvcid("
                              << i->vc->channel_id << ")\n";
                }

            vc->pause_audio (false);

            try
                {
                    player_manager->update_info_embed (vc->server_id);
                }
            catch (...)
                {
                }

            i = rt_v.erase (i);
        }
}

static bool
is_fprt_duplicate (const failed_playback_reset_timer_t &rt)
{
    auto i = fprt_v.begin ();

    while (i != fprt_v.end ())
        {
            if (i->filename != rt.filename)
                {
                    i++;
                    continue;
                }

            return true;
        }

    return false;
}

int
create_failed_playback_reset_timer (const std::string &filename)
{
    if (filename.empty ())
        return 1;

    std::lock_guard lk (fprt_m);

    failed_playback_reset_timer_t rt = { util::get_current_ts (), filename };

    bool no = is_fprt_duplicate (rt);
    if (no)
        return 2;

    fprt_v.push_back (rt);

    return 0;
}

int
remove_failed_playback_reset_timer (const std::string &filename)
{
    if (filename.empty ())
        return 1;

    std::lock_guard lk (fprt_m);

    auto i = fprt_v.begin ();

    while (i != fprt_v.end ())
        {
            if (i->filename != filename)
                {
                    i++;
                    continue;
                }

            fprt_v.erase (i);
            break;
        }

    return 0;
}

void
check_failed_playback_reset_timers ()
{
    bool exiting = !get_running_state ();
    /*bool debug = get_debug_state ();*/

    std::lock_guard lk (fprt_m);

    auto now = util::get_current_ts ();
    auto i = fprt_v.begin ();
    while (i != fprt_v.end ())
        {
            if (exiting)
                {
                    i = fprt_v.erase (i);
                    continue;
                }

            constexpr long long m10 = util::ms_to_picos (60 * 10000);

            auto diff = (now - i->ts);
            // less than 10 minute passed
            if (diff < m10)
                {
                    i++;
                    continue;
                }

            i = fprt_v.erase (i);
        }
}

} // musicat::player
