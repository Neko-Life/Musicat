#include "musicat/mctrack.h"
#include "musicat/YTDLPTrack.h"
#include "musicat/player.h"

namespace musicat::mctrack
{
int
get_track_flag (const player::MCTrack &track)
{
    const nlohmann::json &data = track.raw;

    auto data_end = data.end ();
    int flag = player::TRACK_MC;

    if (data.find ("ie_key") != data_end)
        flag = flag & player::TRACK_YTDLP_SEARCH;

    if (data.find ("extractor_key") != data_end)
        flag = flag & player::TRACK_YTDLP_DETAILED;

    return flag;
}

bool
is_YTDLPTrack (const player::MCTrack &track)
{
    int flag = get_track_flag (track);

    return is_YTDLPTrack (flag);
}

bool
is_YTDLPTrack (int track_flag)
{
    return track_flag & player::TRACK_YTDLP_SEARCH
           || track_flag & player::TRACK_YTDLP_DETAILED;
}

std::string
get_title (const player::MCTrack &track)
{
    if (is_YTDLPTrack (track))
        return YTDLPTrack::get_title (track);

    return track.title ();
}

std::string
get_title (const yt_search::YTrack &track)
{
    return track.title ();
}

} // musicat::mctrack
