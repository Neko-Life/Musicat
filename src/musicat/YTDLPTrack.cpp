#include "musicat/YTDLPTrack.h"
#include "musicat/musicat.h"
#include "musicat/player.h"

namespace musicat::YTDLPTrack
{
std::string
get_str (const nlohmann::json &data, const std::string &at)
{
    if (!data.is_object ())
        return "";

    auto i = data.find (at);

    if (i == data.end () || !i->is_string ())
        return "";

    return i->get<std::string> ();
}

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
    return get_str (data, "id");
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

    return get_str (data, "title");
}

uint64_t
get_duration (const nlohmann::json &data)
{
    if (!data.is_object ())
        return 0;

    auto i = data.find ("duration");

    if (i == data.end () || i->is_null ())
        return 0;

    double v = i->get<double> ();

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
        return get_str (data, "thumbnail");

    const nlohmann::json &info_data = track.info.raw;

    if (info_data.is_object () && is_detailed (info_data))
        return get_str (info_data, "thumbnail");

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

    return get_str (data, "description");
}

std::string
get_url (const player::MCTrack &track)
{
    const nlohmann::json &data = track.raw;

    std::string url = get_str (data, "url");

    if (url.empty ())
        return get_str (data, "original_url");

    return url;
}

std::string
get_length_str (const player::MCTrack &track)
{
    uint64_t dur = get_duration (track);

    return format_duration (dur);
}

std::string
get_channel_name (const player::MCTrack &track)
{
    const nlohmann::json &data = track.raw;

    std::string type = get_str (data, "extractor");

    if (type == "generic")
        {
            return get_str (data, "webpage_url_domain");
        }

    return get_str (data, "channel");
}

std::vector<player::MCTrack>
get_playlist_entries (const nlohmann::json &data)
{
    if (!data.is_object ())
        {
            return {};
        }

    auto ep = data.find ("entries");
    if (ep == data.end () || !ep->is_array ())
        {
            return {};
        }

    std::vector<player::MCTrack> ret = {};

    for (const auto &e : *ep)
        {
            player::MCTrack t;
            t.raw = e;
            ret.push_back (t);
        }

    return ret;
}

} // musicat::YTDLPTrack
