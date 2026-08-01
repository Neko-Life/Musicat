#include "musicat/player_manager_timer.h"
#include "musicat/musicat.h"
#include "musicat/player.h"
#include "musicat/util.h"

namespace musicat::player::timer
{
/* static std::vector<connect_timer_t> ct_v; */
static std::vector<resume_timer_t> rt_v;
static std::vector<failed_playback_reset_timer_t> fprt_v;

/* static std::mutex ct_m; */
static std::mutex rt_m;
static std::mutex fprt_m;

void
check_connect_timers ()
{
    // !TODO
}

static bool
is_rt_duplicate (const resume_timer_t &rt)
{
    auto i = rt_v.begin ();

    while (i != rt_v.end ())
        {
            bool sids = i->svid != rt.svid;
            bool svcids = i->svcid != rt.svcid;
            bool cids = i->vcid != rt.vcid;

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
create_resume_timer (const dpp::snowflake &user_id,
                     const dpp::snowflake &user_voice_channel_id,
                     dpp::discord_voice_client *vc, long long min_delay)
{
    if (!vc)
        return 1;

    auto player_manager = get_player_manager_ptr ();
    if (!player_manager || player_manager->is_manually_paused (vc->server_id))
        {
            return 2;
        }

    std::lock_guard lk (rt_m);

    resume_timer_t rt
        = { util::get_current_ts (), user_id,  vc->server_id, vc->channel_id,
            user_voice_channel_id,   min_delay };

    if (get_debug_state ())
        {
            std::cerr << "[player::timer::create_resume_timer] "
                         "Creating resume_timer_t: ts("
                      << rt.ts << ") uid(" << rt.uid << ") svcid(" << rt.svcid
                      << ") vcsid(" << rt.svid << ") vcvcid(" << rt.vcid
                      << ")\n";
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

            auto server_id = i->svid;

            auto vgid = get_voice_from_gid (server_id, i->uid);

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
                              << ") vcsid(" << i->svid << ") vcvcid("
                              << i->vcid << ")\n";
                }

            auto guild_player = player_manager->get_player (server_id);

            if (guild_player)
                {
                    player_manager->unpause (guild_player->get_voice_client (),
                                             server_id, true);
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

            // actually reset the track count
            set_track_failed_playback_count (i->filename, 0);

            i = fprt_v.erase (i);
        }
}

} // musicat::player
