#include "musicat/util/base64.h"
#include "cppcodec/base64_rfc4648.hpp"
#include "cppcodec/base64_url_unpadded.hpp"

namespace musicat::util::base64
{
std::string
encode (const std::string &str)
{
    return cppcodec::base64_url_unpadded::encode (str);
}

std::string
encode_standard (const std::string &str)
{
    return cppcodec::base64_rfc4648::encode (str);
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

std::string
decode_standard (const std::string &encoded_str)
{
    auto res = cppcodec::base64_rfc4648::decode (encoded_str);
    std::string ret;
    ret.reserve (res.size ());

    for (const unsigned char &c : res)
        {
            ret += c;
        }

    return ret;
}
} // musicat::util::base64
