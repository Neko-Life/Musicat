/* Very simple YouTube track searching function. Made with love by Neko Life :heart: */

#include <string>
#include <vector>
#include "nlohmann/json.hpp"
#include "musicat/encode.h"
#include "musicat/yt-search.h"

namespace yt_search {
    using json = nlohmann::json;

    std::string YTrack::snippetText() {
        std::string res = "";
        json d = raw["detailedMetadataSnippets"][0]["snippetText"]["runs"];
        if (d[0].is_null()) return res;

        for (size_t i = 0; i < d.size(); i++)
        {
            json data = d[i];
            bool bold = data["bold"].is_null() ? false : data["bold"].get<bool>();
            std::string text = data["text"].is_null() ? "" : data["text"].get<std::string>();

            if (bold) res += "**" + text + "**";
            else res += text;
        }
        return res;
    }

    std::string YTrack::id() {
        return raw["videoId"].is_null() ? "" : raw["videoId"].get<std::string>();
    }

    std::string YTrack::url() {
        return std::string("https://www.youtube.com/watch?v=") + id();
    }

    std::string YTrack::trackingParams() {
        return raw["trackingParams"].is_null() ? "" : raw["trackingParams"].get<std::string>();
    }

    std::vector<YThumbnail> YTrack::thumbnails() {
        std::vector<YThumbnail> res;
        json d = raw["thumbnail"]["thumbnails"];
        if (d[0].is_null()) return res;
        for (size_t i = 0; i < d.size(); i++)
        {
            json data = d[i];
            res.push_back({
                data["height"].get<int>(),
                data["url"].get<std::string>(),
                data["width"].get<int>()
                });
        }
        return res;
    }

    YThumbnail YTrack::bestThumbnail() {
        return thumbnails().back();
    }

    // Track length
    // TODO: Parse to ms
    std::string YTrack::length() {
        json res = raw["lengthText"]["simpleText"];
        if (res.is_null()) return "";
        return res.get<std::string>();
    }

    YChannel YTrack::channel() {
        json d = raw["longBylineText"]["runs"][0];
        if (d.is_null()) return {};
        json m = d["navigationEndpoint"]["commandMetadata"]["webCommandMetadata"]["url"];
        return {
            d["text"].is_null() ? "" : d["text"].get<std::string>(),
            m.is_null() ? "" : std::string("https://www.youtube.com") + m.get<std::string>()
        };
    }

    std::string YTrack::title() {
        json d = raw["title"]["runs"][0]["text"];
        if (d.is_null()) if ((d = raw["title"]["simpleText"]).is_null()) return "";
        return d.get<std::string>();
    }

    // This is interesting
    std::string YSearchResult::estimatedResults() {
        return raw["estimatedResults"];
    }

    std::vector<YTrack> YSearchResult::trackResults() {
        std::vector<YTrack> res;
        json d = raw["contents"]["twoColumnSearchResultsRenderer"]["primaryContents"]["sectionListRenderer"]["contents"][0]["itemSectionRenderer"]["contents"];
        if (d[0].is_null()) return res;

        for (size_t i = 0; i < d.size(); i++)
        {
            json data = d[i]["videoRenderer"];
            if (data.is_null()) continue;
            res.push_back({
                data
                });
        }
        return res;
    }

    YSearchResult search(std::string search) {
        YSearchResult ret;
        get_data(std::string("https://www.youtube.com/results?search_query=") + encodeURIComponent(search), &ret);
        return ret;
    }
}
