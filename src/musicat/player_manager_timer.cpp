#include "musicat/player_manager_timer.h"
#include "musicat/musicat.h"
#include "musicat/util.h"

namespace musicat::player::timer
{
static std::vector<connect_timer_t> ct_v;
static std::vector<resume_timer_t> rt_v;

static std::mutex ct_m;
static std::mutex rt_m;

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
            bool sids = i->vc->server_id == rt.vc->server_id;
            bool svcids = i->svcid == rt.svcid;
            bool cids = i->vc->channel_id == rt.vc->channel_id;

            if (sids && svcids && cids)
                return true;

            i++;
        }

    return false;
}

int
create_resume_timer (const dpp::snowflake &user_id,
                     const dpp::snowflake &user_voice_channel_id,
                     dpp::discord_voice_client *vc)
{
    if (!vc)
        return 1;

    std::lock_guard lk (rt_m);

    resume_timer_t rt
        = { util::get_current_ts (), user_id, user_voice_channel_id, vc };

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

    auto i = rt_v.begin ();
    while (i != rt_v.end ())
        {
            if (exiting || !player_manager)
                {
                    i = rt_v.erase (i);
                    continue;
                }

            auto now = util::get_current_ts ();
            auto diff = (now - i->ts);
            // less than 2 second passed
            if (diff < 1500000000LL)
                {
                    i++;
                    continue;
                }

            auto vc = i->vc;

            if (!vc)
                {
                    i = rt_v.erase (i);
                    continue;
                }

            auto vgid = get_voice_from_gid (vc->server_id, i->uid);

            if (vc->terminating || !vgid.first || vgid.first->id != i->svcid)
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

} // musicat::player
