#include "musicat/yt-track-info.h"
#include "musicat/yt-search.h"
#include "nlohmann/json.hpp"
#include <ostream>
#include <string>

namespace yt_search
{
using json = nlohmann::json;

// "audio/webm; codecs=\"opus\""
// "video/mp4; codecs=\"avc1.4d401f\""

mime_type_t::mime_type_t () : str ("") {}

mime_type_t::mime_type_t (std::string str)
{
    if (!str.length ())
        {
            this->str = "";
            return;
        }
    this->str = str;
    size_t l_type_index = str.find ('/');
    if (l_type_index)
        {
            this->type = str.substr (0, l_type_index);
            size_t l_format_index = str.find (';');
            if (l_format_index)
                this->format = str.substr (
                    l_type_index + 1, l_format_index - (l_type_index + 1));
        }
    size_t l_codecs_index = str.find ('"');
    if (l_codecs_index)
        this->codecs = str.substr (l_codecs_index + 1,
                                   str.length () - l_codecs_index - 2);
}

mime_type_t::~mime_type_t () = default;

int
audio_info_t::itag ()
{
    return raw["itag"].get<int> ();
}

std::string
audio_info_t::url ()
{
    if (raw["url"].is_null ())
        return "";
    return raw["url"].get<std::string> ();
}

mime_type_t
audio_info_t::mime_type ()
{
    std::string con = "";
    if (!raw["mimeType"].is_null ())
        con = raw["mimeType"].get<std::string> ();
    return mime_type_t (con);
}

size_t
audio_info_t::bitrate ()
{
    if (raw["bitrate"].is_null ())
        return 0;
    return raw["bitrate"].get<size_t> ();
}

std::pair<size_t, size_t>
get_ranges (const json &raw, const std::string &key)
{
    if (raw[key].is_null ())
        return { 0, 0 };
    json re1 = raw[key]["start"];
    json re2 = raw[key]["end"];
    size_t r1 = 0;
    size_t r2 = 0;
    if (!re1.is_null ())
        {
            std::istringstream op (re1.get<std::string> ());
            op >> r1;
        }
    if (!re2.is_null ())
        {
            std::istringstream op (re2.get<std::string> ());
            op >> r2;
        }
    return { r1, r2 };
}

std::pair<size_t, size_t>
audio_info_t::init_range ()
{
    return get_ranges (raw, "initRange");
}

std::pair<size_t, size_t>
audio_info_t::index_range ()
{
    return get_ranges (raw, "indexRange");
}

time_t
audio_info_t::last_modified ()
{
    time_t ret = 0;
    if (!raw["lastModified"].is_null ())
        {
            std::istringstream s (raw["lastModified"].get<std::string> ());
            s >> ret;
        }
    return ret;
}

size_t
audio_info_t::length ()
{
    size_t ret = 0;
    if (!raw["contentLength"].is_null ())
        {
            std::istringstream s (raw["contentLength"].get<std::string> ());
            s >> ret;
        }
    return ret;
}

std::string
audio_info_t::quality ()
{
    if (raw["quality"].is_null ())
        return "";
    return raw["quality"].get<std::string> ();
}

std::string
audio_info_t::projection_type ()
{
    if (raw["projectionType"].is_null ())
        return "";
    return raw["projectionType"].get<std::string> ();
}

size_t
audio_info_t::average_bitrate ()
{
    if (raw["averageBitrate"].is_null ())
        return 0;
    return raw["averageBitrate"].get<size_t> ();
}

std::string
audio_info_t::audio_quality ()
{
    if (raw["audioQuality"].is_null ())
        return "";
    return raw["audioQuality"].get<std::string> ();
}

uint64_t
audio_info_t::duration ()
{
    uint64_t ret = 0;
    if (!raw["approxDurationMs"].is_null ())
        {
            std::istringstream s (raw["approxDurationMs"].get<std::string> ());
            s >> ret;
        }
    return ret;
}

uint64_t
audio_info_t::sample_rate ()
{
    uint64_t ret = 0;
    if (!raw["audioSampleRate"].is_null ())
        {
            std::istringstream s (raw["audioSampleRate"].get<std::string> ());
            s >> ret;
        }
    return ret;
}

uint8_t
audio_info_t::channel ()
{
    if (raw["audioChannels"].is_null ())
        return 0;
    return raw["audioChannels"].get<int> ();
}

double
audio_info_t::loudness ()
{
    if (raw["loudnessDb"].is_null ())
        return 0;
    return raw["loudnessDb"].get<double> ();
}

audio_info_t
YTInfo::audio_info (int format)
{
    audio_info_t ret;
    auto l = raw["streamingData"]["adaptiveFormats"];
    if (l.is_null ())
        {
            printf ("PATH IS NULL\n");
            return ret;
        }
    for (size_t j = 0; j < l.size (); j++)
        {
            json i = l[j];
            if (i["itag"].is_null ())
                continue;
            int a = i["itag"].get<int> ();
            if (a == format)
                {
                    ret.raw = i;
                    break;
                }
        }
    return ret;
}

YTInfo
get_track_info (std::string url)
{
    YTInfo ret;
    get_data (url, &ret, "var ytInitialPlayerResponse = ");
    return ret;
}
}
