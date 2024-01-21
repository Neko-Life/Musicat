#include "musicat/util/base32.h"
#include <cppcodec/base64_url_unpadded.hpp>

namespace musicat::util::base32
{
std::string
encode (const std::string &str)
{
    return cppcodec::base64_url_unpadded::encode (str);
}

std::string
decode (const std::string &encoded_str)
{
    auto res = cppcodec::base64_url_unpadded::decode (encoded_str);
    std::string ret;
    ret.reserve (res.size ());

    for (const unsigned char &c : res)
        {
            ret += c;
        }

    return ret;
}
} // musicat::util::base32
