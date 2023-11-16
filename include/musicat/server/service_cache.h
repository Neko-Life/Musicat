#ifndef MUSICAT_SERVER_SERVICE_CACHE_H
#define MUSICAT_SERVER_SERVICE_CACHE_H

#include "nlohmann/json.hpp"
#include <dpp/dpp.h>
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

void handle_guild_create (const dpp::guild_create_t &e);

void handle_guild_delete (const dpp::guild_delete_t &e);

} // musicat::server::service_cache

#endif // MUSICAT_SERVER_SERVICE_CACHE_H
