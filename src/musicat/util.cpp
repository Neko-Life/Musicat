#include "musicat/util.h"
#include "musicat/musicat.h"
#include <stdint.h>
#include <string>

#include <unicode/utypes.h>
#include <unicode/uchar.h>
#include <unicode/locid.h>
#include <unicode/ustring.h>
#include <unicode/ucnv.h>
#include <unicode/unistr.h>

namespace musicat
{
namespace util
{
std::string
join (const bool join, const std::string &str, const std::string &join_str)
{
    return join ? str + join_str : str;
}

void
u8_limit_length (const char *unicode_str, char *buf, int32_t max_length)
{
    icu_72::UnicodeString unicodeKey (unicode_str);

    const bool debug = get_debug_state ();

    auto v_s = unicodeKey.extract (0, max_length, buf, max_length);
    buf[max_length] = '\0';

    if (debug)
        {
            printf ("[util::u8_limit_length] need_length buf "
                    "extracted_length max_length: '%d' '%s' "
                    "'%ld' '%d'\n",
                    v_s, buf, strnlen(buf, max_length), max_length);
        }
}

} // util
} // musicat
