#ifndef MUSICAT_UTIL_H
#define MUSICAT_UTIL_H

#include "musicat/player.h"
#include <memory>
#include <mutex>

namespace musicat::util
{
/**
 * @brief Join str with join_str if join is true
 */
std::string join (const bool join, const std::string &str, const std::string &join_str);

std::string u8_limit_length (const char *unicode_str, std::string &out, int32_t max_length = 99);

void print_autocomplete_results (const std::vector<std::pair<std::string, std::string> > &avail, const char *debug_fn);

/**
 * @brief Convert unix timestamp to ISO8601 string
 */
std::string time_t_to_ISO8601 (time_t &timer);

/**
 * @brief Fuzzy find `search` in `str`
 */
bool fuzzy_match (std::string search, std::string str, const bool case_insensitive = false);

uint64_t get_random_number ();

bool is_player_not_playing (std::shared_ptr<player::Player> &guild_player, dpp::voiceconn *voiceconn);

template <typename T>
T
rand_item (const std::vector<T> &vec)
{
    uint64_t r = get_random_number ();

    // vec MUST NOT be empty!
    uint64_t idx = r % vec.size ();

    return vec.at (idx);
}

/**
 * @brief Get member highest role, with_color by default
 */
dpp::role *get_user_highest_role (const dpp::snowflake &guild_id, const dpp::snowflake &user_id, bool with_color = true);

/**
 * @brief Whether numstr is valid number and parse-able to integer types.
 * @return 0 if true, -1 if no length and found invalid char on invalid
 */
char valid_number (const std::string &numstr);

void log_confirmation_error (const dpp::confirmation_callback_t &e, const char *callee = "ERROR");

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

find_focused_t<dpp::command_option> find_focused (const std::vector<dpp::command_option> &options,
                                                  const std::vector<std::string> &paths = {});

find_focused_t<dpp::command_data_option> find_focused (const std::vector<dpp::command_data_option> &options,
                                                       const std::vector<std::string> &paths = {});

std::string trim_str (const std::string &str);

std::string max_len (const std::string &s, size_t max_len, bool cut_front = false);

using ret_hook_fn = void (*) (void *data);

class ret_hook_t
{
    void *data;
    ret_hook_fn hook;

  public:
    ret_hook_t () : data (nullptr), hook (nullptr) {}
    ret_hook_t (void *data, ret_hook_fn hook) : data (data), hook (hook) {}

    ~ret_hook_t ()
    {
        if (hook)
            hook (data);
    }
};

class throttler_t
{
    int c;
    int max_c;
    std::condition_variable c_cv;
    std::mutex c_m;

    struct throttle_sync_t
    {
        throttler_t *t;
        throttle_sync_t (throttler_t *_t) : t (_t)
        {
            std::unique_lock lk (t->c_m);
            while (t->c >= t->max_c)
                t->c_cv.wait (lk);
            t->c++;
        }

        ~throttle_sync_t ()
        {
            {
                std::lock_guard lk (t->c_m);
                t->c--;
            }
            t->c_cv.notify_one ();
        }
    };

  public:
    throttler_t (int max_run = 2) : c (0), max_c (max_run) {}

    throttle_sync_t
    throttle ()
    {
        return throttle_sync_t{ this };
    }

    bool
    will_block ()
    {
        std::lock_guard lk (c_m);
        return (c >= max_c);
    }
};

} // musicat::util

#endif // MUSICAT_UTIL_H
