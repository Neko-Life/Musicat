#ifndef MUSICAT_H
#define MUSICAT_H

#include "musicat/player.h"
#include "nekos-best++.hpp"
#include <dpp/dpp.h>
#include <stdexcept>
#include <string>
#include <vector>

namespace musicat
{
extern nlohmann::json sha_cfg;

// Main
int run (int argc, const char *argv[]);

/**
 * @brief Get current running state
 */
bool get_running_state ();

/**
 * @brief Set current running state, setting to false will cause program to
 * perform clean up and exit
 *
 * @return int 0 on success
 */
int set_running_state (const bool state);

/**
 * @brief Get current debug state
 */
bool get_debug_state ();

/**
 * @brief Set current debug state, setting to true will enable verbose logging
 *
 * @return int 0 on success
 */
int set_debug_state (const bool state);

/**
 * @brief Get config value of key
 *
 * @param key
 * @param default_value
 *
 * @return T
 */
template <typename T>
T
get_config_value (const std::string &key, const T& default_value)
{
    if (sha_cfg.is_null())
    {
        fprintf(stderr, "[ERROR] Config isn't populated\n");
        return default_value;
    }
    if (!sha_cfg.is_object())
    {
        fprintf(stderr, "[ERROR] Invalid config, config isn't object\n");
        return default_value;
    }

    return sha_cfg.value(key, default_value);
}

/**
 * @brief Get music folder path
 *
 * @return std::string
 */
std::string
get_music_folder_path ();

/**
 * @brief Get bot description
 *
 * @return std::string
 */
std::string
get_bot_description ();

/**
 * @brief Search _find inside _vec
 *
 * @param _vec
 * @param _find
 *
 * @return std::vector<T>::iterator iterator to _find
 */
template <typename T>
typename std::vector<T>::iterator
vector_find (std::vector<T> *_vec, T _find)
{
    auto i = _vec->begin ();
    for (; i != _vec->end (); i++)
        {
            if (*i == _find)
                return i;
        }
    return i;
}

/**
 * @brief Destroy and reset connecting state of the guild, must be invoked when
 * failed to join or left a vc
 *
 * @param client The client
 * @param guild_id Guild Id of the vc client connecting in
 * @param delete_voiceconn Whether to delete found voiceconn, can cause
 * segfault if the underlying structure doesn't exist
 */
void reset_voice_channel (dpp::discord_client *client, dpp::snowflake guild_id,
                          bool delete_voiceconn = false);

/**
 * @brief Get the voice object and connected voice members a vc of a guild
 *
 * @param guild Guild the member in
 * @param user_id Target member
 * @return std::pair<dpp::channel*, std::map<dpp::snowflake, dpp::voicestate>>
 * @throw const char* User isn't in vc
 */
std::pair<dpp::channel *, std::map<dpp::snowflake, dpp::voicestate> >
get_voice (dpp::guild *guild, dpp::snowflake user_id);

/**
 * @brief Get the voice object and connected voice members a vc of a guild
 *
 * @param guild_id Guild Id the member in
 * @param user_id Target member
 * @return std::pair<dpp::channel*, std::map<dpp::snowflake, dpp::voicestate>>
 * @throw const char* Unknown guild or user isn't in vc
 */
std::pair<dpp::channel *, std::map<dpp::snowflake, dpp::voicestate> >
get_voice_from_gid (dpp::snowflake guild_id, dpp::snowflake user_id);

/**
 * @brief Execute shell cmd and return anything it printed to console
 *
 * @param cmd Command
 * @return std::string
 * @throw const char* Exec failed (can't call popen or unknown command)
 */
std::string exec (std::string cmd);

bool has_listener (std::map<dpp::snowflake, dpp::voicestate> *vstate_map);

bool
has_listener_fetch (dpp::cluster *client,
                    std::map<dpp::snowflake, dpp::voicestate> *vstate_map);

template <typename T, typename E>
void
get_inter_param (const E &event, std::string param_name, T *param)
{
    auto p = event.get_parameter (param_name);
    if (p.index ())
        *param = std::get<T> (p);
}

class exception : std::exception
{
  private:
    const char *message;
    int c;

  public:
    exception (const char *_message, int _code = 0);

    virtual const char *what () const noexcept;

    virtual int code () const noexcept;
};

int cli (dpp::cluster &client, dpp::snowflake sha_id, int argc,
         const char *argv[]);

bool has_permissions (dpp::guild *guild, dpp::user *user,
                      dpp::channel *channel,
                      std::vector<uint64_t> permissions = {});
bool has_permissions_from_ids (dpp::snowflake guild_id, dpp::snowflake user_id,
                               dpp::snowflake channel_id,
                               std::vector<uint64_t> permissions = {});

/**
 * @brief Format ms duration to HH:MM:SS
 *
 * @param dur
 * @return std::string
 */
std::string format_duration (uint64_t dur);

/**
 * @brief Get random index of len to use as base to shuffle an array
 *
 * @param len
 * @return std::vector<size_t>
 */
std::vector<size_t> shuffle_indexes (size_t len);

/**
 * @brief Attempt to join voice channel
 *
 * @param from
 * @param player_manager
 * @param guild_id
 * @param user_id
 * @param sha_id
 *
 * @return int 0 if request to connect sent
 */
int join_voice (dpp::discord_client *from,
                player::player_manager_ptr player_manager,
                const dpp::snowflake &guild_id, const dpp::snowflake &user_id,
                const dpp::snowflake &sha_id);

nekos_best::endpoint_map
get_cached_nekos_best_endpoints ();

/**
 * @brief Get client ptr, returns nullptr if program exiting
 * probably should be careful in thread
 */
dpp::cluster *
get_client_ptr ();

/**
 * @brief Get client id
 */
dpp::snowflake
get_sha_id ();

/**
 * @brief Get player manager shared_ptr, won't be available in cli context
 */
player::player_manager_ptr
get_player_manager_ptr ();

}

#endif
