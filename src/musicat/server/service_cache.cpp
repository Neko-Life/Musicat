#include "nlohmann/json.hpp"
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
    return service_cache::get (user_id + "/auth");
}

void
set_cached_user_auth (const std::string &user_id, const nlohmann::json &data)
{
    std::string key = user_id + "/auth";
    service_cache::set (key, data);

    // 5 minutes
    service_cache::create_invalidate_timer (key, 300);
}

nlohmann::json
get_cached_user_guilds (const std::string &user_id)
{
    return service_cache::get (user_id + "/get_user_guilds");
}

void
set_cached_user_guilds (const std::string &user_id, const nlohmann::json &data)
{
    std::string key = user_id + "/get_user_guilds";
    service_cache::set (key, data);

    // 5 minutes
    service_cache::create_invalidate_timer (key, 300);
}

} // musicat::server::service_cache
