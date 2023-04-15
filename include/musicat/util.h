#ifndef MUSICAT_UTIL_H
#define MUSICAT_UTIL_H

#include <string>
#include <vector>

namespace musicat
{
namespace util
{
/**
 * @brief Join str with join_str if join is true
 */
std::string join (const bool join, const std::string &str,
                  const std::string &join_str);

void u8_limit_length (const char *unicode_str, char *buf, int32_t max_length = 99);

void print_autocomplete_results (const std::vector<std::pair<std::string, std::string>> &avail,
                                 const char *debug_fn);

/**
 * @brief Convert unix timestamp to ISO8601 string
 */
std::string
time_t_to_ISO8601 (time_t &timer);

/**
 * @brief Fuzzy find `search` in `str`
 */
bool
fuzzy_match (std::string search, std::string str, const bool case_insensitive = false);

} // util
} // musicat

#endif // MUSICAT_UTIL_H
