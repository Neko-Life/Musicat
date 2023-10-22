#ifndef MUSICAT_UTIL_RESPONSE_H
#define MUSICAT_UTIL_RESPONSE_H

#include "musicat/player.h"
#include <deque>
#include <string>

namespace musicat::util::response
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

dpp::component create_short_text_input (const std::string &label,
                                        const std::string &id);

} // musicat::util::response

#endif // MUSICAT_UTIL_RESPONSE_H
