#include <string>
#include "nlohmann/json.hpp"
#include "yt-search/yt-search.h"
#include "yt-search/yt-track-info.h"

namespace yt_search {
    using json = nlohmann::json;

    // "audio/webm; codecs=\"opus\""
    // "video/mp4; codecs=\"avc1.4d401f\""

    mime_type_t::mime_type_t() : str("") {}

    mime_type_t::mime_type_t(std::string str) {
        if (!str.length())
        {
            this->str = "";
            return;
        }
        this->str = str;
        size_t l_type_index = str.find('/');
        if (l_type_index)
        {
            this->type = str.substr(0, l_type_index);
            size_t l_format_index = str.find(';');
            if (l_format_index) this->format = str.substr(l_type_index + 1, l_format_index - (l_type_index + 1));
        }
        size_t l_codecs_index = str.find('"');
        if (l_codecs_index) this->codecs = str.substr(l_codecs_index + 1, str.length() - l_codecs_index - 2);
    }

    mime_type_t::~mime_type_t() = default;

    int audio_info_t::itag() const {
        return raw.is_null() ? 0 : raw.value<int>("itag", 0);
    }

    std::string audio_info_t::url() const {
        return raw.is_null() ? "" : raw.value<std::string>("url", "");
    }

    mime_type_t audio_info_t::mime_type() const {
        return mime_type_t(raw.is_null() ? "" : raw.value<std::string>("mimeType", ""));
    }

    size_t audio_info_t::bitrate() const {
        return raw.is_null() ? 0 : raw.value<size_t>("bitrate", 0);
    }

    std::pair<size_t, size_t> get_ranges(const json& raw, const std::string& key) {
        if (raw.is_null()) return { 0,0 };

        json j = raw;

        if (!transverse(j, { key.c_str() })) return { 0,0 };

        json re1 = j;//"start"
        json re2 = j;//"end"
        size_t r1 = 0;
        size_t r2 = 0;

        if (transverse(re1, { "start" }))
        {
		r1 = strtoul(re1.get<std::string>().c_str(), NULL, 10);
        }
        if (transverse(re2, { "end" }))
        {
		r2 = strtoul(re2.get<std::string>().c_str(), NULL, 10);
        }

        return { r1,r2 };
    }

    std::pair<size_t, size_t> audio_info_t::init_range() const {
        return get_ranges(raw, "initRange");
    }

    std::pair<size_t, size_t> audio_info_t::index_range() const {
        return get_ranges(raw, "indexRange");
    }

    time_t audio_info_t::last_modified() const {
        time_t ret = 0;
        if (!raw.is_null())
        {
		ret = (time_t)strtol(raw.value<std::string>("lastModified", "").c_str(), NULL, 10);
        }
        return ret;
    }

    size_t audio_info_t::length() const {
        size_t ret = 0;
        if (!raw.is_null())
        {
            ret = strtoul(raw.value<std::string>("contentLength", "").c_str(), NULL, 10);
        }
        return ret;
    }

    std::string audio_info_t::quality() const {
        return raw.is_null() ? "" : raw.value<std::string>("quality", "");
    }

    std::string audio_info_t::projection_type() const {
        return raw.is_null() ? "" : raw.value<std::string>("projectionType", "");
    }

    size_t audio_info_t::average_bitrate() const {
        return raw.is_null() ? 0 : raw.value<uint64_t>("averageBitrate", 0);
    }

    std::string audio_info_t::audio_quality() const {
        return raw.is_null() ? "" : raw.value<std::string>("audioQuality", "");
    }

    uint64_t audio_info_t::duration() const {
        uint64_t ret = 0;
        if (!raw.is_null())
        {
            ret = strtoul(raw.value<std::string>("approxDurationMs", "").c_str(), NULL, 10);
        }
        return ret;
    }

    uint64_t audio_info_t::sample_rate() const {
        uint64_t ret = 0;
        if (!raw.is_null())
        {
            ret = strtoul(raw.value<std::string>("audioSampleRate", "").c_str(), NULL, 0);
        }
        return ret;
    }

    uint8_t audio_info_t::channel() const {
        return raw.is_null() ? 0 : raw.value<uint64_t>("audioChannels", 0);
    }

    double audio_info_t::loudness() const {
        return raw.is_null() ? 0.0D : raw.value<double>("loudnessDb", 0.0D);
    }

    audio_info_t YTInfo::audio_info(int format) const {
        audio_info_t ret;
        json l = raw;
	
        if (!transverse(l, { "streamingData", "adaptiveFormats" }))
        {
            return ret;
        }
        
	if (l.is_array()) for (size_t j = 0; j < l.size(); j++)
        {
            json i = l.at(j);
	    if (i.is_null()) continue;
            
	    int a = i.value<int64_t>("itag", -1);

            if (a == format)
            {
                ret.raw = i;
                break;
            }
        }
        return ret;
    }

    YTInfo get_track_info(std::string url) {
        YTInfo ret;
        get_data(url, &ret, "var ytInitialPlayerResponse = ");
        return ret;
    }
}
