#include "musicat/search-cache.h"
#include "yt-search/yt-search.h"
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace musicat
{
namespace search_cache
{
static search_cache_map_t cache = {};
static std::mutex cache_m;

track_v_t
get (const std::string &id)
{
    std::lock_guard lk (cache_m);

    auto i = cache.find (id);
    if (i == cache.end ())
        return {};

    return i->second;
}

int
set (const std::string &id, const track_v_t &val)
{
    std::lock_guard lk (cache_m);

    cache.insert_or_assign (id, val);
    return 0;
}

size_t
remove (const std::string &id)
{
    std::lock_guard lk (cache_m);

    return cache.erase (id);
}

} // search_cache
} // musicat
