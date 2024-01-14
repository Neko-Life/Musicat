#ifndef MUSICAT_MCTRACK_H
#define MUSICAT_MCTRACK_H

/*
    Wrapper for compatibility between MCTrack and YTDLPTrack

    Decided to keep YDLPTrack a raw json only that's simply contained
    in the raw field of MCTrack
*/

#include "musicat/player.h"

namespace musicat::mctrack
{
struct search_option_t
{
    const std::string &query;
    const int max_entries;
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

/**
 * @Brief Search a query or fetch a playlist playlist url
 *        or even get the detail of a track
 *        This is blocking call that WILL block your thread
 *        for a good amount of time
 */
std::vector<player::MCTrack> fetch (const search_option_t &options);

} // musicat::mctrack

#endif // MUSICAT_MCTRACK_H
