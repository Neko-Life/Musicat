#ifndef MUSICAT_PLAYER_MANAGER_TIMER_H
#define MUSICAT_PLAYER_MANAGER_TIMER_H

#include <dpp/dpp.h>

namespace musicat::player::timer
{

struct connect_timer_t
{
    long long ts;
    dpp::snowflake gid;
    // !TODO
};

struct resume_timer_t
{
    long long ts;
    dpp::snowflake uid;
    dpp::snowflake svcid;
    dpp::discord_voice_client *vc;
    long long min_delay;
};

struct track_marker_rm_timer_t
{
    long long ts;
    std::string meta;
    dpp::discord_voice_client *vc;
};

int create_track_marker_rm_timer (const std::string &meta,
                                  dpp::discord_voice_client *vc);

int create_resume_timer (const dpp::snowflake &user_id,
                         const dpp::snowflake &user_voice_channel_id,
                         dpp::discord_voice_client *vc,
                         long long min_delay = 1500000000LL);

void check_track_marker_rm_timers ();

void check_resume_timers ();

} // musicat::player::timer

#endif // MUSICAT_PLAYER_MANAGER_TIMER_H
