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

} // musicat::YTDLPTrack
