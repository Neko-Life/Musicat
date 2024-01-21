#ifndef MUSICAT_UTIL_BASE32_H
#define MUSICAT_UTIL_BASE32_H

#include <string>

namespace musicat::util::base32
{
std::string encode (const std::string &str);

std::string decode (const std::string &encoded_str);
} // musicat::util::base32

#endif // MUSICAT_UTIL_BASE32_H
