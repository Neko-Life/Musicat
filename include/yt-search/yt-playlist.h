#ifndef YT_PLAYLIST_H
#define YT_PLAYLIST_H

#include <vector>
#include <string>
#include "yt-search/yt-search.h"
#include "nlohmann/json.hpp"

namespace yt_search {
    struct YPlaylist {
        nlohmann::json raw;
        std::vector<YTrack> entries() const;
    };

    YPlaylist get_playlist(std::string url);
}

#endif
