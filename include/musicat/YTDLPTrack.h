#ifndef MUSICAT_YTDLPTRACK_H
#define MUSICAT_YTDLPTRACK_H

#include "musicat/player.h"

namespace musicat::YTDLPTrack
{
bool is_detailed (const nlohmann::json &raw);

std::string get_id_nocheck (const nlohmann::json &data);

std::string get_id (const nlohmann::json &data);
std::string get_id (const player::MCTrack &track);

std::string get_title (const player::MCTrack &track);

uint64_t get_duration (const player::MCTrack &track);
uint64_t get_duration (const yt_search::audio_info_t &info);
uint64_t get_duration (const nlohmann::json &data);

std::string get_thumbnail (const player::MCTrack &track);

std::string get_description (const player::MCTrack &track);

std::string get_url (const player::MCTrack &track);

} // musicat::YTDLPTrack

#endif // MUSICAT_YTDLPTRACK_H
