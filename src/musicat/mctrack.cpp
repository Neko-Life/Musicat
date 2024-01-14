#include "musicat/mctrack.h"
#include "musicat/YTDLPTrack.h"
#include "musicat/player.h"

namespace musicat::mctrack
{
int
get_track_flag (const nlohmann::json &data)
{
    if (!data.is_object ())
        return 0;

    auto data_end = data.end ();
    int flag = player::TRACK_MC;

    if (data.find ("ie_key") != data_end)
        flag = flag & player::TRACK_YTDLP_SEARCH;

    if (data.find ("extractor_key") != data_end)
        flag = flag & player::TRACK_YTDLP_DETAILED;

    return flag;
}

int
get_track_flag (const player::MCTrack &track)
{
    const nlohmann::json &data = track.raw;

    return get_track_flag (data);
}

int
get_track_flag (const yt_search::audio_info_t &info)
{
    const nlohmann::json &data = info.raw;

    return get_track_flag (data);
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

bool
is_YTDLPTrack (const yt_search::audio_info_t &info)
{
    int flag = get_track_flag (info);

    return is_YTDLPTrack (flag);
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

uint64_t
get_duration (const player::MCTrack &track)
{
    if (is_YTDLPTrack (track))
        return YTDLPTrack::get_duration (track);

    if (is_YTDLPTrack (track.info))
        return YTDLPTrack::get_duration (track.info);

    return track.info.duration ();
}

std::string
get_thumbnail (player::MCTrack &track)
{
    if (is_YTDLPTrack (track))
        return YTDLPTrack::get_thumbnail (track);

    return track.bestThumbnail ().url;
}

std::string
get_description (const player::MCTrack &track)
{
    if (is_YTDLPTrack (track))
        return YTDLPTrack::get_description (track);

    return track.snippetText ();
}

} // musicat::mctrack
