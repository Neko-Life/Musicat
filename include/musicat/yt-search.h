/* Very simple YouTube track searching function. Made with love by Neko Life :heart: */

#ifndef YT_SEARCH_H
#define YT_SEARCH_H

#include "nlohmann/json.hpp"

using json = nlohmann::json;

struct YChannel {
    std::string name;
    std::string url;
};

struct YThumbnail {
    int height;
    std::string url;
    int width;
};

struct YTrack {
    json raw;

    std::string snippetText();
    std::string id();
    std::string url();
    std::string trackingParams();
    std::vector<YThumbnail> thumbnails();
    YThumbnail bestThumbnail();

    // Track length
    // TODO: Parse to ms
    std::string length();
    YChannel channel();
    std::string title();
};

struct YSearchResult {
    json raw;

    // This is interesting
    std::string estimatedResults();
    std::vector<YTrack> trackResults();
    size_t count();
};

/**
 * @brief Search and return track results
 *
 * @param search Query
 * @return YSearchResult
 */
YSearchResult yt_search(std::string search);

#endif
