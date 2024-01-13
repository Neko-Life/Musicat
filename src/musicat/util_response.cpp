#include "musicat/util_response.h"
#include "musicat/mctrack.h"
#include "musicat/musicat.h"

namespace musicat::util::response
{
std::string
str_queue_position (const int64_t &position)
{
    return position == 1  ? "the top of "
                            "the queue"
           : position > 1 ? "position " + std::to_string (position)
                          : "the end of the queue";
}

std::string
reply_downloading_track (const std::string &title)
{
    return std::string ("Downloading `") + title + "`... Gimme 10 sec ight";
}

std::string
reply_added_track (const std::string &title, const int64_t &position)
{
    if (get_debug_state ())
        fprintf (stderr, "track position: '%s' %ld\n", title.c_str (),
                 position);

    return std::string ("Added `") + title + "` to "
           + str_queue_position (position);
}

std::string
reply_added_playlist (const std::string &playlist_name,
                      const int64_t &position, const int64_t &count)
{
    return std::string ("Added ") + std::to_string (count) + " track"
           + (count > 1 ? "s" : "") + " from playlist `" + playlist_name
           + "` to " + str_queue_position (position);
}

std::string
reply_skipped_track (std::deque<musicat::player::MCTrack> &removed_tracks)
{
    auto removed_size = removed_tracks.size ();

    std::string append_response = "";

    if (removed_size)
        {
            switch (removed_size)
                {
                case 1:
                    append_response
                        += " `" + mctrack::get_title (removed_tracks.front ())
                           + "`";
                    break;
                case 2:
                    append_response
                        += " `" + mctrack::get_title (removed_tracks.front ())
                           + "` and `"
                           + mctrack::get_title (removed_tracks.at (1)) + "`";
                    break;
                default:
                    const size_t rc = removed_size - 2;
                    append_response
                        += " `" + mctrack::get_title (removed_tracks.front ())
                           + "`, `"
                           + mctrack::get_title (removed_tracks.at (1))
                           + "` and " + std::to_string (rc) + " other track"
                           + (rc > 1 ? "s" : "");
                }
        }

    return std::string ("Skipped") + append_response;
}

std::string
str_mention_user (const dpp::snowflake &user_id)
{
    return std::string ("<@") + std::to_string (user_id) + std::string ("> ");
}

dpp::component
create_short_text_input (const std::string &label, const std::string &id)
{
    dpp::component component;
    component.set_type (dpp::cot_text)
        .set_label (label)
        .set_id (id)
        .set_text_style (dpp::text_short);

    return component;
}

} // musicat::util::response
