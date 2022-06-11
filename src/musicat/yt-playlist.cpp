#include <vector>
#include "nlohmann/json.hpp"
#include "musicat/yt-playlist.h"

namespace yt_search {
    using json = nlohmann::json;
    std::vector<YTrack> YPlaylist::entries() {
        std::vector<YTrack> res;
        json d = raw;
        for (auto i : { "contents","twoColumnWatchNextResults","playlist","playlist","contents" })
        {
            // printf("%s\n\n", d[i].dump(2).c_str());
            if (d[i].is_null())
            {
                printf("d[%s].is_null\n", i);
                return res;
            }
            d = d[i];
        }

        for (size_t i = 0; i < d.size(); i++)
        {
            json data = d[i]["playlistPanelVideoRenderer"];
            if (data.is_null())
            {
                printf("data[%ld].is_null\n", i);
                continue;
            }
            res.push_back({
                data
                });
        }
        return res;
    }

    YPlaylist get_playlist(std::string url) {
        YPlaylist ret;
        get_data(url, &ret);
        return ret;
    }
}
