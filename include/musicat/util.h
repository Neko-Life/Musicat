#ifndef MUSICAT_UTIL_H
#define MUSICAT_UTIL_H

#include <string>

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

} // util
} // musicat

#endif // MUSICAT_UTIL_H
