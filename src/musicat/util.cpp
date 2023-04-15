#include "musicat/musicat.h"
#include "musicat/util.h"
#include <stdint.h>
#include <string>

#include <unicode/locid.h>
#include <unicode/uchar.h>
#include <unicode/ucnv.h>
#include <unicode/unistr.h>
#include <unicode/ustring.h>
#include <unicode/utypes.h>

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
                    v_s, buf, strnlen (buf, max_length * 4), max_length);
        }
}

void
print_autocomplete_results (
    const std::vector<std::pair<std::string, std::string> > &avail,
    const char *debug_fn)
{
    printf ("[%s] results:\n", debug_fn);
    for (size_t i = 0; i < avail.size (); i++)
        {
            printf ("%ld: %s\n", i, avail.at (i).first.c_str ());
        }
}

std::string
time_t_to_ISO8601 (time_t &timer)
{
    struct tm *timeinfo;
    char buffer[128];

    timeinfo = localtime (&timer);

    strftime (buffer, 127, "%FT%T%z", timeinfo);
    buffer[127] = '\0';

    return std::string (buffer);
}

bool
fuzzy_match (std::string search, std::string str, const bool case_insensitive)
{
    bool match = true;

    auto i = str.begin ();
    for (const char &sc : search)
        {
            bool found = false;

            while (i != str.end ())
                {
                    if (case_insensitive
                            ? std::tolower (*i) == std::tolower (sc)
                            : *i == sc)
                        found = true;

                    i++;

                    if (found)
                        break;
                }

            if (!found)
                {
                    match = false;
                    break;
                }
        }

    return match;
}

} // util
} // musicat
