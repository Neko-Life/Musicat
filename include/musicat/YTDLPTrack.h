#ifndef MUSICAT_YTDLPTRACK_H
#define MUSICAT_YTDLPTRACK_H

#include "musicat/player.h"

namespace musicat::YTDLPTrack
{
bool is_detailed (const nlohmann::json &raw);

std::string get_title (const player::MCTrack &track);

} // musicat::YTDLPTrack

#endif // MUSICAT_YTDLPTRACK_H
