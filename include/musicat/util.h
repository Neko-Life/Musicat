#ifndef MUSICAT_UTIL_H
#define MUSICAT_UTIL_H

#include "musicat/player.h"
#include <dpp/dpp.h>
#include <string>
#include <vector>

namespace musicat
{
namespace util
{
/**
 * @brief Join str with join_str if join is true
 */
std::string join (const bool join, const std::string &str,
                  const std::string &join_str);

void u8_limit_length (const char *unicode_str, char *buf,
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

bool
is_player_not_playing (std::shared_ptr<player::Player> &guild_player,
                       dpp::voiceconn *voiceconn);

namespace response
{

std::string str_queue_position (const int64_t &position);

std::string reply_downloading_track (const std::string &title);

std::string reply_added_track (const std::string &title,
                               const int64_t &position = 0);

std::string reply_added_playlist (const std::string &playlist_name,
                                  const int64_t &position,
                                  const int64_t &count);

std::string
reply_skipped_track (std::deque<musicat::player::MCTrack> &removed_tracks);

std::string str_mention_user (const dpp::snowflake &user_id);

} // response
} // util
} // musicat

#endif // MUSICAT_UTIL_H
