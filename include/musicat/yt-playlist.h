#ifndef YT_PLAYLIST_H
#define YT_PLAYLIST_H

#include <vector>
#include <string>
#include "musicat/yt-search.h"
#include "nlohmann/json.hpp"

namespace yt_search {
    struct YPlaylist {
        nlohmann::json raw;
        std::vector<YTrack> entries();
    };

    YPlaylist get_playlist(std::string url);
}

#endif
