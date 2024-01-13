#include "musicat/mctrack.h"
#include "musicat/YTDLPTrack.h"

namespace musicat::mctrack
{
bool
is_YTDLPTrack (const player::MCTrack &track)
{
}

std::string
get_title (const player::MCTrack &track)
{
    if (is_YTDLPTrack (track))
        return YTDLPTrack::get_title (track);

    return track.title ();
}

} // musicat::mctrack
