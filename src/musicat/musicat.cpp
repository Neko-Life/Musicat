#include "musicat/musicat.h"
#include "musicat/cmds.h"
#include "musicat/util.h"
#include <chrono>
#include <dpp/discordclient.h>
#include <mutex>
#include <vector>

namespace musicat
{
using string = std::string;

/**
 * @brief Destroy and reset connecting state of the guild, must be invoked when
 * failed to join or left a vc
 *
 * @param client The client
 * @param guild_id Guild Id of the vc client connecting in
 * @param delete_voiceconn Whether to delete found voiceconn, can cause
 * segfault if the underlying structure doesn't exist
 */
void
reset_voice_channel (dpp::discord_client *client, dpp::snowflake guild_id,
                     bool delete_voiceconn)
{
    auto v = client->connecting_voice_channels.find (guild_id);
    if (v == client->connecting_voice_channels.end ())
        return;

    // unused since dpp update, probably can be removed
    // if (v->second && delete_voiceconn)
    //     {
    //         delete v->second;
    //         v->second = nullptr;
    //     }
    client->connecting_voice_channels.erase (v);
}

/**
 * @brief Get the voice object and connected voice members a vc of a guild
 *
 * @param guild Guild the member in
 * @param user_id Target member
 * @return std::pair<dpp::channel*, std::map<dpp::snowflake, dpp::voicestate>>
 * @throw const char* User isn't in vc
 */
std::pair<dpp::channel *, std::map<dpp::snowflake, dpp::voicestate> >
get_voice (dpp::guild *guild, dpp::snowflake user_id)
{
    for (auto &fc : guild->channels)
        {
            auto gc = dpp::find_channel (fc);
            if (!gc || (!gc->is_voice_channel () && !gc->is_stage_channel ()))
                continue;

            std::map<dpp::snowflake, dpp::voicestate> vm
                = gc->get_voice_members ();

            if (vm.find (user_id) != vm.end ())
                {
                    return { gc, vm };
                }
        }

    return { NULL, {} };
}

/**
 * @brief Get the voice object and connected voice members a vc of a guild
 *
 * @param guild_id Guild Id the member in
 * @param user_id Target member
 * @return std::pair<dpp::channel*, std::map<dpp::snowflake, dpp::voicestate>>
 * @throw const char* Unknown guild or user isn't in vc
 */
std::pair<dpp::channel *, std::map<dpp::snowflake, dpp::voicestate> >
get_voice_from_gid (dpp::snowflake guild_id, dpp::snowflake user_id)
{
    dpp::guild *g = dpp::find_guild (guild_id);
    if (g)
        return get_voice (g, user_id);

    return { NULL, {} };
}

bool
has_listener (std::map<dpp::snowflake, dpp::voicestate> *vstate_map)
{
    if (!vstate_map || vstate_map->size () <= 1)
        return false;

    for (const std::pair<const dpp::snowflake, dpp::voicestate> &r :
         *vstate_map)
        {
            const dpp::snowflake uid = r.second.user_id;
            if (!uid)
                continue;

            dpp::user *u = dpp::find_user (uid);

            if (!u || u->is_bot ())
                continue;

            return true;
        }

    return false;
}

bool
has_listener_fetch (dpp::cluster *client,
                    std::map<dpp::snowflake, dpp::voicestate> *vstate_map)
{
    if (vstate_map->size () < 2)
        return false;

    for (auto r : *vstate_map)
        {
            auto u = dpp::find_user (r.second.user_id);
            /* if (!u)
                {
                    try
                        {
                            if (get_debug_state ())
                                printf ("Fetching user %ld in vc %ld\n",
                                        r.second.user_id, r.second.channel_id);
                            dpp::user_identified uf
                                = client->user_get_sync (r.second.user_id);
                            if (uf.is_bot ())
                                continue;
                        }
                    catch (const dpp::rest_exception *e)
                        {
                            fprintf (stderr,
                                     "[ERROR(musicat.182)] Can't "
                                     "fetch user in VC: %ld\n",
                                     r.second.user_id);
                            continue;
                        }
                }
            else */
            if (u->is_bot ())
                continue;

            return true;
        }

    return false;
}

exception::exception (const char *_message, int _code)
{
    message = _message;
    c = _code;
}

const char *
exception::what () const noexcept
{
    return message;
}

int
exception::code () const noexcept
{
    return c;
}

bool
has_permissions (dpp::guild *guild, dpp::user *user, dpp::channel *channel,
                 const std::vector<uint64_t> &permissions)
{
    if (!guild || !user || !channel)
        return false;

    uint64_t p = guild->permission_overwrites (guild->base_permissions (user),
                                               user, channel);
    for (uint64_t i : permissions)
        if (!(p & i))
            return false;

    return true;
}

bool
has_permissions_from_ids (const dpp::snowflake &guild_id,
                          const dpp::snowflake &user_id,
                          const dpp::snowflake &channel_id,
                          const std::vector<uint64_t> &permissions)
{
    if (!guild_id || !user_id || !channel_id)
        return false;

    return has_permissions (dpp::find_guild (guild_id),
                            dpp::find_user (user_id),
                            dpp::find_channel (channel_id), permissions);
}

string
format_duration (uint64_t dur)
{
    uint64_t secondr = dur / 1000;
    uint64_t minuter = secondr / 60;

    uint64_t hour = minuter / 60;
    uint8_t minute = minuter % 60;
    uint8_t second = secondr % 60;

    string ret = "";
    if (hour)
        {
            string hstr = std::to_string (hour);
            ret += string (hstr.length () < 2 ? "0" : "") + hstr + ":";
        }

    string mstr = std::to_string (minute);
    string sstr = std::to_string (second);

    ret += string (mstr.length () < 2 ? "0" : "") + mstr + ":"
           + string (sstr.length () < 2 ? "0" : "") + sstr;

    return ret;
}

std::vector<size_t>
shuffle_indexes (size_t len)
{
    srand (util::get_current_ts ());

    std::vector<size_t> ret = {};
    ret.reserve (len);

    std::vector<size_t> ori = {};
    ori.reserve (len);

    for (size_t i = 0; i < len; i++)
        ori.push_back (i);

    auto io = ori.begin ();
    while (io != ori.end ())
        {
            int r = rand () % ori.size ();
            auto it = io + r;

            ret.push_back (*it);
            ori.erase (it);
        }

    size_t s = ret.size ();
    if (s != len)
        fprintf (stderr, "[WARN(musicat.209)] Return size %ld != %ld\n", s,
                 len);

    return ret;
}

int
join_voice (dpp::discord_client *from,
            player::player_manager_ptr_t player_manager,
            const dpp::snowflake &guild_id, const dpp::snowflake &user_id,
            const dpp::snowflake &sha_id)
{

    std::pair<dpp::channel *, std::map<dpp::snowflake, dpp::voicestate> > c,
        c2;

    c = get_voice_from_gid (guild_id, user_id);

    // user isn't in voice channel
    if (!c.first)
        return 1;

    // no channel cached
    if (!c.first->id)
        return 3;

    // client voice state
    c2 = get_voice_from_gid (guild_id, sha_id);

    // client already in a guild session
    if (has_listener (&c2.second))
        return 2;

    if (!has_permissions_from_ids (guild_id, sha_id, c.first->id,
                                   { dpp::p_view_channel, dpp::p_connect }))
        // no permission
        return 4;

    player_manager->full_reconnect (
        from, guild_id,
        (c2.first && c2.first->id) ? c2.first->id : dpp::snowflake (0),
        c.first->id);

    // success dispatching join voice channel
    return 0;
}

void
close_valid_fd (int *i)
{
    if (*i == -1)
        return;

    close (*i);

    *i = -1;
}

} // musicat
