#include "musicat/autocomplete.h"
#include "musicat/musicat.h"
#include <cstring>
#include <dpp/dpp.h>
#include <unicode/utypes.h>
#include <unicode/uchar.h>
#include <unicode/locid.h>
#include <unicode/ustring.h>
#include <unicode/ucnv.h>
#include <unicode/unistr.h>

namespace musicat
{
namespace autocomplete
{
std::vector<std::pair<std::string, std::string> >
filter_candidates (
    const std::vector<std::pair<std::string, std::string> > &candidates,
    std::string param)
{
    std::vector<std::pair<std::string, std::string> > avail = {};
    avail.reserve (25U);

    for (const std::pair<std::string, std::string> &i : candidates)
        {
            const std::string ori = i.first;
            std::string i2 = i.first;
            for (char &j : i2)
                j = std::tolower (j);
            for (char &j : param)
                j = std::tolower (j);
            if (i2.find (param) != std::string::npos)
                avail.push_back (std::make_pair (ori, i.second));
            if (avail.size () == 25U)
                break;
        }

    return avail;
}

void
create_response (
    const std::vector<std::pair<std::string, std::string> > &avail,
    const dpp::autocomplete_t &event)
{
    dpp::interaction_response r (dpp::ir_autocomplete_reply);

    const bool b_25 = avail.size () > 25U;

    for (const std::pair<std::string, std::string> &i : avail)
        {
            // get unicode chars right
            char v[512];
            char v2[512];

            memset (v, '\0', sizeof (v));
            memset (v2, '\0', sizeof (v2));
            /* static char v[100]; */
            /* static char v2[100]; */
            icu_72::UnicodeString unicodeKey (i.first.c_str ());
            icu_72::UnicodeString unicodeVal (i.second.c_str ());
            /* auto uk_length = unicodeKey.length (); */
            /* auto uv_length = unicodeVal.length (); */

            /* for (int i = 0; i < (uk_length < 99UL ? (int)uk_length : 99); i++)
             */
            /*     { */
            /*         v[i] = unicodeKey.charAt(i); */
            /*     } */
            /* v[99] = 0; */

            /* for (int i = 0; i < (uv_length < 99UL ? (int)uv_length : 99); i++)
             */
            /*     { */
            /*         v2[i] = unicodeKey.charAt(i); */
            /*     } */
            /* v2[99] = 0; */
            static const int32_t max_length = 99;
            int32_t e = max_length;
            int32_t e2 = max_length;

            const bool debug = get_debug_state ();

            int ie = 1;
            while ((e = unicodeKey.extract (0, e, NULL)) > max_length)
                {
                    if (debug)
                        printf ("[autocomplete::create_response] while e ie: '%d' '%d'\n",
                                e, ie);
                    e -= ie++;
                }
            int ie2 = 1;
            while ((e2 = unicodeKey.extract (0, e2, NULL)) > max_length)
                {
                    if (debug)
                        printf ("[autocomplete::create_response] while e2 ie2: '%d' '%d'\n",
                                e2, ie2);
                    e2 -= ie2++;
                }

            auto v_s = unicodeKey.extract (0, e, v);
            auto v2_s = unicodeVal.extract (0, e2, v2);
            /* v[v_s] = 0; */
            /* v2[v2_s] = 0; */

            if (debug)
                {
                    /* auto v_s = sizeof(v); */
                    /* auto v2_s = sizeof(v2); */
                    printf ("[autocomplete::create_response] v_s v e: '%d' '%s' "
                            "'%d'\n",
                            v_s, v, e);
                    printf ("[autocomplete::create_response] v2_s v2 e2: '%d' "
                            "'%s' '%d'\n",
                            v2_s, v2, e2);
                }

            /* std::string v */
            /*     = i.first.length () > 100 ? i.first.substr (0, 100) : i.first;
             */
            /* std::string v2 */
            /*     = i.second.length () > 100 ? i.second.substr (0, 100) :
             * i.second; */
            r.add_autocomplete_choice (dpp::command_option_choice (v, v2));
            if (b_25 && r.autocomplete_choices.size () == 25U)
                break;
        }
    event.from->creator->interaction_response_create (event.command.id,
                                                      event.command.token, r);
}
} // namespace autocomplete
} // namespace musicat
