#include "musicat/YTDLPTrack.h"

namespace musicat::YTDLPTrack
{
bool
is_detailed (const nlohmann::json &raw)
{
    return raw.find ("extractor_key") != raw.end ();
}

std::string
get_title (const player::MCTrack &track)
{
    const nlohmann::json &data = track.raw;
    return data.value ("title", "");
}

uint64_t
get_duration (const nlohmann::json &data)
{
    if (!data.is_object ())
        return 0;

    double v = data.value ("duration", 0.0);

    return v > 0.0 ? (uint64_t)(v * 1000) : (uint64_t)v;
}

uint64_t
get_duration (const player::MCTrack &track)
{
    const nlohmann::json &data = track.raw;

    return get_duration (data);
}

uint64_t
get_duration (const yt_search::audio_info_t &info)
{
    const nlohmann::json &data = info.raw;

    return get_duration (data);
}

} // musicat::YTDLPTrack
