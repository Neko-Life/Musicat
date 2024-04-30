#ifndef MUSICAT_UTIL_H
#define MUSICAT_UTIL_H

#include "musicat/player.h"
#include <memory>

namespace musicat::util
{
/**
 * @brief Join str with join_str if join is true
 */
std::string join (const bool join, const std::string &str,
                  const std::string &join_str);

std::string u8_limit_length (const char *unicode_str, std::string &out,
                             int32_t max_length = 99);

void print_autocomplete_results (
    const std::vector<std::pair<std::string, std::string> > &avail,
    const char *debug_fn);

/**
 * @brief Convert unix timestamp to ISO8601 string
 */
std::string time_t_to_ISO8601 (time_t &timer);

/**
 * @brief Fuzzy find `search` in `str`
 */
bool fuzzy_match (std::string search, std::string str,
                  const bool case_insensitive = false);

int get_random_number ();

bool is_player_not_playing (std::shared_ptr<player::Player> &guild_player,
                            dpp::voiceconn *voiceconn);

// vec size can't be larger than (RAND_MAX+1)
template <typename T>
T
rand_item (const std::vector<T> &vec)
{
    int r = get_random_number ();

    int idx = r > 0 ? (r % vec.size ()) : 0;

    return vec.at (idx);
}

/**
 * @brief Get member highest role, with_color by default
 */
dpp::role *get_user_highest_role (const dpp::snowflake &guild_id,
                                  const dpp::snowflake &user_id,
                                  bool with_color = true);

/**
 * @brief Whether numstr is valid number and parse-able to integer types.
 * @return 0 if true, -1 if no length and found invalid char on invalid
 */
char valid_number (const std::string &numstr);

void log_confirmation_error (const dpp::confirmation_callback_t &e,
                             const char *callee = "ERROR");

// returns current timestamp in picosecond
long long get_current_ts ();

inline constexpr long long
ms_to_picos (const long long ms)
{
    return ms * 1000000LL;
}

template <typename T> struct find_focused_t
{
    T focused;
    std::vector<std::string> paths;
};

find_focused_t<dpp::command_option>
find_focused (const std::vector<dpp::command_option> &options,
              const std::vector<std::string> &paths = {});

find_focused_t<dpp::command_data_option>
find_focused (const std::vector<dpp::command_data_option> &options,
              const std::vector<std::string> &paths = {});

std::string trim_str (const std::string &str);

} // musicat::util

#endif // MUSICAT_UTIL_H
