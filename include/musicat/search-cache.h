#include "yt-search/yt-search.h"
#include <map>
#include <string>
#include <vector>

namespace musicat
{
namespace search_cache
{
using track_v_t = yt_search::YSearchResult;
using search_cache_map_t = std::map<std::string, track_v_t>;

track_v_t get (const std::string &id);

int set (const std::string &id, const track_v_t &val);

size_t remove (const std::string &id);

} // search_cache
} // musicat
