#ifndef MUSICAT_UTIL_BASE64_H
#define MUSICAT_UTIL_BASE64_H

#include <string>

namespace musicat::util::base64
{
std::string encode (const std::string &str);

std::string decode (const std::string &encoded_str);
} // musicat::util::base64

#endif // MUSICAT_UTIL_BASE64_H
