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
bool is_YTDLPTrack (const player::MCTrack &track);

std::string get_title (const player::MCTrack &track);

} // musicat::mctrack

#endif // MUSICAT_MCTRACK_H
