/* Very simple YouTube track searching function. Made with love by Neko Life :heart: */

#include "nlohmann/json.hpp"
#include "musicat/encode.h"
#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include "musicat/yt-search.h"
using json = nlohmann::json;

std::string YTrack::snippetText() {
    std::string res;
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
    json d = raw["ownerText"]["runs"][0];
    if (d.is_null()) return {};
    json m = d["navigationEndpoint"]["commandMetadata"]["webCommandMetadata"]["url"];
    return {
        d["text"].is_null() ? "" : d["text"].get<std::string>(),
        m.is_null() ? "" : std::string("https://www.youtube.com") + m.get<std::string>()
    };
}

std::string YTrack::title() {
    json d = raw["title"]["runs"][0]["text"];
    if (d.is_null()) return "";
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

size_t YSearchResult::count() {
    return trackResults().size();
}

YSearchResult yt_search(std::string search) {
    std::ostringstream os;

    curlpp::Cleanup curl_cleanup;

    curlpp::Easy req;

    req.setOpt(curlpp::options::Url(std::string("https://www.youtube.com/results?search_query=") + encodeURIComponent(std::string(search))));
    req.setOpt(curlpp::options::Header("User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:98.0) Gecko/20100101 Firefox/98.0"));
    req.setOpt(curlpp::options::WriteStream(&os));
    req.perform();

    // MAGIC INIT
    std::string rawhttp = os.str();

    const char var[21] = "var ytInitialData = ";
    const char end[3] = "};";

    bool sw = false;
    size_t sI = -1;
    size_t eI = -1;

    // MAGIC EXECUTE
    for (size_t i = 0; i < rawhttp.length(); i++)
    {
        if (sw == false)
            for (size_t v = 0; v < sizeof(var) - 1; v++)
            {
                if (rawhttp[i + v] != var[v]) break;
                if (v == sizeof(var) - 2)
                {
                    sI = i + sizeof(var) - 1;
                    sw = true;
                }
            }
        else for (size_t v = 0; v < sizeof(end) - 1; v++)
        {
            if (rawhttp[i + v] != end[v]) break;
            if (v)
            {
                eI = i + sizeof(end) - 2;
                break;
            }
        }
        if ((int)eI > -1) break;
    }

    YSearchResult data;
    if ((int)sI < 0 || (int)eI < 0)
    {
        // If this getting printed to the console, the magic may be expired
        printf("Not a valid youtube search query (or youtube update, yk they like to change stuffs)\nvar_start: %ld\nvar_end: %ld\n", sI, eI);
        return data;
    }

    // MAGIC FINALIZE
    std::string tJson = rawhttp.replace(rawhttp.begin() + eI, rawhttp.end(), "").replace(rawhttp.begin(), rawhttp.begin() + sI, "");
    data.raw = json::parse(tJson);
    return data;
}
