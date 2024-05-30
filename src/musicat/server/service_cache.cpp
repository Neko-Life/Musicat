#include "musicat/server/service_cache.h"
#include "musicat/musicat.h"
#include "nlohmann/json.hpp"
#include <dpp/dpp.h>
#include <map>
#include <mutex>

// can be used for db cache too
namespace musicat::server::service_cache
{

struct invalidate_timer_t
{
    long long ts;
    int second_max_age;
    std::string key;
};

std::map<std::string, nlohmann::json> _cache;
std::mutex cache_m;

std::vector<invalidate_timer_t> _timers;
std::mutex _timers_m;

void
set (const std::string &key, const nlohmann::json &value)
{
    std::lock_guard lk (cache_m);

    _cache.insert_or_assign (key, value);
}

nlohmann::json
get (const std::string &key)
{
    std::lock_guard lk (cache_m);

    auto i = _cache.find (key);
    if (i == _cache.end ())
        {
            return nullptr;
        }

    return i->second;
}

void
remove (const std::string &key)
{
    std::lock_guard lk (cache_m);

    auto i = _cache.find (key);
    if (i == _cache.end ())
        {
            return;
        }

    _cache.erase (i);
}

void
create_invalidate_timer (const std::string &key, int second_max_age)
{
    std::lock_guard lk (_timers_m);

    _timers.push_back ({ std::chrono::high_resolution_clock::now ()
                             .time_since_epoch ()
                             .count (),
                         second_max_age, key });
}

void
check_timers ()
{
    std::lock_guard lk (_timers_m);

    auto i = _timers.begin ();
    while (i != _timers.end ())
        {
            long long current_ts = std::chrono::high_resolution_clock::now ()
                                       .time_since_epoch ()
                                       .count ();

            long long passed = (current_ts - i->ts);
            long long max = ((long long)i->second_max_age * 1000000000LL);

            if (passed > max)
                {
                    remove (i->key);
                    i = _timers.erase (i);
                    continue;
                }

            i++;
        }
}

void
remove_invalidate_timer (const std::string &key)
{
    std::lock_guard lk (_timers_m);

    auto i = _timers.begin ();
    while (i != _timers.end ())
        {
            if (i->key != key)
                {
                    i++;
                    continue;
                }

            _timers.erase (i);
            return;
        }
}

nlohmann::json
get_cached_user_auth (const std::string &user_id)
{
    return get (user_id + "/auth");
}

void
set_cached_user_auth (const std::string &user_id, const nlohmann::json &data)
{
    std::string key = user_id + "/auth";
    set (key, data);

    // 5 minutes
    create_invalidate_timer (key, 300);
}

nlohmann::json
get_cached_user_guilds (const std::string &user_id)
{
    return get (user_id + "/get_user_guilds");
}

void
set_cached_user_guilds (const std::string &user_id, const nlohmann::json &data)
{
    std::string key = user_id + "/get_user_guilds";
    set (key, data);

    // now we handle guild event properly and cache invalidation
    // we can store all the cache as long as we want
    // one day
    create_invalidate_timer (key, 86400);
}

void
remove_cached_user_guilds (const std::string &user_id)
{
    std::string key = user_id + "/get_user_guilds";

    remove (key);
    remove_invalidate_timer (key);
}

nlohmann::json
get_cached_musicat_detailed_user ()
{
    const auto shaid = get_sha_id ();

    if (!shaid)
        return nullptr;

    return get (shaid.str () + "/detailed_user");
}

void
set_cached_musicat_detailed_user (const nlohmann::json &data)
{
    const auto shaid = get_sha_id ();

    if (!shaid)
        return;

    std::string key = shaid.str () + "/detailed_user";
    set (key, data);

    // 1 hour
    create_invalidate_timer (key, 3600);
}

// !TODO: handle user update

void
handle_guild_create (const dpp::guild_create_t &e)
{
    if (!e.created || e.created->owner_id.empty ())
        return;

    remove_cached_user_guilds (e.created->owner_id.str ());

    std::lock_guard lk (cache_m);

    auto i = _cache.begin ();
    while (i != _cache.end ())
        {
            size_t idx = i->first.find ("/get_user_guilds");
            if (idx == i->first.npos)
                {
                    i++;
                    continue;
                }

            std::string uid = i->first.substr (0, idx);

            if (e.created->members.find (uid) == e.created->members.end ())
                {
                    i++;
                    continue;
                }

            remove_invalidate_timer (i->first);
            i = _cache.erase (i);
        }
}

void
handle_guild_delete (const dpp::guild_delete_t &e)
{
    if (e.deleted.owner_id.empty ())
        return;

    remove_cached_user_guilds (e.deleted.owner_id.str ());

    std::lock_guard lk (cache_m);

    auto i = _cache.begin ();
    while (i != _cache.end ())
        {
            size_t idx = i->first.find ("/get_user_guilds");
            if (idx == i->first.npos)
                {
                    i++;
                    continue;
                }

            std::string uid = i->first.substr (0, idx);

            if (e.deleted.members.find (uid) == e.deleted.members.end ())
                {
                    i++;
                    continue;
                }

            remove_invalidate_timer (i->first);
            i = _cache.erase (i);
        }
}

// !TODO: handle guild member add/delete

} // musicat::server::service_cache
