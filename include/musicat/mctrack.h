#ifndef MUSICAT_MCTRACK_H
#define MUSICAT_MCTRACK_H

/*
    Wrapper for compatibility between MCTrack and YTDLPTrack

    Decided to keep YDLPTrack a raw json only that's simply contained
    in the raw field of MCTrack
*/

#define YDLP_DEFAULT_MAX_ENTRIES 20

#include "snowflake.h"
#include "yt-search/yt-search.h"
#include "yt-search/yt-track-info.h"

namespace musicat::player
{

enum track_flag_t
{
    TRACK_MC = 0,
    TRACK_YTDLP_SEARCH = 1,
    TRACK_YTDLP_DETAILED = (1 << 1),
    TRACK_SHORT = (1 << 2),
};

struct MCTrack : yt_search::YTrack
{
    /**
     * @brief Downloaded track file path.
     *
     */
    std::string filename;

    /**
     * @brief User Id which added this track.
     *
     */
    dpp::snowflake user_id;

    /**
     * @brief List of user voted for this message
     *
     */
    std::deque<dpp::snowflake> skip_vote;

    yt_search::audio_info_t info;

    bool seekable;

    int64_t repeat;

    // seek query, reset to empty string after seek performed.
    // ffmpeg -ss value
    std::string seek_to;

    // current byte position
    int64_t current_byte;

    bool is_empty () const;

    void init ();

    MCTrack ();
    explicit MCTrack (const yt_search::YTrack &t);
    ~MCTrack ();

    void check_for_seek_to ();
};
} // musicat::player

namespace musicat::mctrack
{
struct search_option_t
{
    const std::string &query;
    // should always be populated, default is 20
    const int max_entries;
    // should always be populated
    const bool is_url;
};

int get_track_flag (const player::MCTrack &track);
int get_track_flag (const yt_search::audio_info_t &info);
int get_track_flag (const nlohmann::json &data);

bool is_YTDLPTrack (const player::MCTrack &track);
bool is_YTDLPTrack (int track_flag);
bool is_YTDLPTrack (const yt_search::audio_info_t &info);

std::string get_title (const player::MCTrack &track);
std::string get_title (const yt_search::YTrack &track);

uint64_t get_duration (const player::MCTrack &track);

std::string get_thumbnail (player::MCTrack &track);

std::string get_description (const player::MCTrack &track);

std::string get_url (const player::MCTrack &track);
std::string get_url (const yt_search::YTrack &track);

std::string get_id (const player::MCTrack &track);
std::string get_id (const yt_search::YTrack &track);

std::string get_length_str (const player::MCTrack &track);

std::string get_channel_name (const player::MCTrack &track);

/**
 * Whether a string has "/shorts/" in it
 */
bool is_url_shorts (const std::string_view &str);

/**
 * Can only detect ytdlp json entry, yt-search doesn't provide actual url but
 * only construct it from its id therefore it will be always false
 */
bool is_short (const nlohmann::json &data);
bool is_short (const player::MCTrack &track);

// see if the next call to fetch() will block for a while
bool fetch_will_block ();

/**
 * @Brief Search a query or fetch a playlist url
 *        or get the detail of a track
 *        This is blocking call that WILL block your thread
 *        for a good amount of time (it uses python!)
 */
nlohmann::json fetch (const search_option_t &options);

} // musicat::mctrack

#endif // MUSICAT_MCTRACK_H
