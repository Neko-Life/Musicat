#ifndef MUSICAT_SERVER_SERVICE_CACHE_H
#define MUSICAT_SERVER_SERVICE_CACHE_H

#include "nlohmann/json.hpp"
#include <mutex>

namespace musicat::server::service_cache
{

/* extern std::mutex cache_m; // EXTERN_VARIABLE */

void set (const std::string &key, const nlohmann::json &value);

nlohmann::json get (const std::string &key);

void remove (const std::string &key);

void create_invalidate_timer (const std::string &key, int second_max_age);
void check_timers ();
void remove_invalidate_timer (const std::string &key);

nlohmann::json get_cached_user_auth (const std::string &user_id);

void set_cached_user_auth (const std::string &user_id,
                           const nlohmann::json &data);

nlohmann::json get_cached_user_guilds (const std::string &user_id);

void set_cached_user_guilds (const std::string &user_id,
                             const nlohmann::json &data);

} // musicat::server::service_cache

#endif // MUSICAT_SERVER_SERVICE_CACHE_H
