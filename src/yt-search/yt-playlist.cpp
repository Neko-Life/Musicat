#include <vector>
#include "nlohmann/json.hpp"
#include "yt-search/yt-playlist.h"

namespace yt_search {
    using json = nlohmann::json;
    std::vector<YTrack> YPlaylist::entries() const {
        std::vector<YTrack> res;
        json d = raw;
        for (auto& i : { "contents","twoColumnWatchNextResults","playlist","playlist","contents" })
        {
            // printf("%s\n\n", d[i].dump(2).c_str());
            d = d.value<json>(i, {});
            if (d.is_null())
            {
                fprintf(stderr, "[YPLAYLIST_ENTRIES] d[%s].is_null\n", i);
                return res;
            }
        }

        if (d.is_array()) for (auto i = d.begin(); i != d.end(); i++)
        {
            json data = i->value<json>("playlistPanelVideoRenderer", {});
            if (data.is_null()) continue;
            else res.push_back({
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
