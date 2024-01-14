#include "musicat/YTDLPTrack.h"

namespace musicat::YTDLPTrack
{
bool
is_detailed (const nlohmann::json &raw)
{
    if (!raw.is_object ())
        return false;

    return raw.find ("extractor_key") != raw.end ();
}

std::string
get_id_nocheck (const nlohmann::json &data)
{
    return data.value ("id", "");
}

std::string
get_id (const nlohmann::json &data)
{
    if (!data.is_object ())
        return "";

    return get_id_nocheck (data);
}

std::string
get_id (const player::MCTrack &track)
{
    const nlohmann::json &data = track.raw;

    return get_id (data);
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

std::string
get_thumbnail (const player::MCTrack &track)
{
    const nlohmann::json &data = track.raw;

    if (is_detailed (data))
        return data.value ("thumbnail", "");

    const nlohmann::json &info_data = track.info.raw;

    if (info_data.is_object () && is_detailed (info_data))
        return info_data.value ("thumbnail", "");

    // !TODO: call ytdlp get info here?

    std::string id = get_id_nocheck (data);

    if (id.empty ())
        return "";

    std::string url_hq_res ("http://i3.ytimg.com/vi/");
    url_hq_res += id + "/hqdefault.jpg";

    return url_hq_res;
}

std::string
get_description (const player::MCTrack &track)
{
    const nlohmann::json &data = track.raw;

    return data.value ("description", "");
}

std::string
get_url (const player::MCTrack &track)
{
    const nlohmann::json &data = track.raw;

    std::string url = data.value ("url", "");

    if (url.empty ())
        return data.value ("original_url", "");

    return url;
}

} // musicat::YTDLPTrack
