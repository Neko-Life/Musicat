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
int get_track_flag (const player::MCTrack &track);
int get_track_flag (const yt_search::audio_info_t &info);
int get_track_flag (const nlohmann::json &data);

bool is_YTDLPTrack (const player::MCTrack &track);
bool is_YTDLPTrack (int track_flag);
bool is_YTDLPTrack (const yt_search::audio_info_t &info);

std::string get_title (const player::MCTrack &track);
std::string get_title (const yt_search::YTrack &track);

uint64_t get_duration (const player::MCTrack &track);

} // musicat::mctrack

#endif // MUSICAT_MCTRACK_H
