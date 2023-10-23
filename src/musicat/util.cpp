#include "musicat/util.h"
#include "musicat/musicat.h"
#include <memory>
#include <unicode/locid.h>
#include <unicode/uchar.h>
#include <unicode/ucnv.h>
#include <unicode/unistr.h>
#include <unicode/ustring.h>
#include <unicode/utypes.h>

namespace musicat::util
{
std::string
join (const bool join, const std::string &str, const std::string &join_str)
{
    return join ? str + join_str : str;
}

void
u8_limit_length (const char *unicode_str, char *buf, int32_t max_length)
{
    icu_73::UnicodeString unicodeKey (unicode_str);

    const bool debug = get_debug_state ();

    auto v_s = unicodeKey.extract (0, max_length, buf, max_length);
    buf[max_length] = '\0';

    if (debug)
        {
            fprintf (stderr,
                     "[util::u8_limit_length] need_length buf "
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
    fprintf (stderr, "[%s] results:\n", debug_fn);
    for (size_t i = 0; i < avail.size (); i++)
        {
            fprintf (stderr, "%ld: %s\n", i, avail.at (i).first.c_str ());
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
    for (char sc : search)
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

int
get_random_number ()
{
    srand (std::chrono::high_resolution_clock::now ()
               .time_since_epoch ()
               .count ());

    return rand ();
}

bool
is_player_not_playing (std::shared_ptr<player::Player> &guild_player,
                       dpp::voiceconn *voiceconn)
{
    return !guild_player || !voiceconn || !voiceconn->voiceclient
           || !voiceconn->voiceclient->is_ready ()
           || (voiceconn->voiceclient->get_secs_remaining () < 0.05f
               && guild_player
               && guild_player->queue.begin () == guild_player->queue.end ());
}

dpp::role *
get_user_highest_role (const dpp::snowflake &guild_id,
                       const dpp::snowflake &user_id, bool with_color)
{
    dpp::guild_member o;
    try
        {
            o = dpp::find_guild_member (guild_id, user_id);
        }
    catch (...)
        {
            // no guild member found
            return nullptr;
        }

    const std::vector<dpp::snowflake> &oroles = o.get_roles ();

    std::vector<dpp::role *> role_list;
    role_list.reserve (oroles.size ());

    for (const auto &i : oroles)
        {
            auto r = dpp::find_role (i);
            if (!r || (with_color && !r->colour))
                continue;

            role_list.push_back (r);
        }

    dpp::role *highest_role = nullptr;
    for (const auto i : role_list)
        {
            if (!i)
                continue;

            if (!highest_role)
                {
                    highest_role = i;
                    continue;
                }

            if (highest_role->position > i->position)
                {
                    continue;
                }

            highest_role = i;
        }

    return highest_role;
}

static inline constexpr const char numbers[] = "1234567890";
static inline constexpr const size_t numbers_siz
    = ((sizeof (numbers) / sizeof (*numbers)) - 1);

char
valid_number (const std::string &numstr)
{
    if (numstr.empty ())
        return -1;

    for (char c : numstr)
        {
            bool valid = false;
            for (size_t i = 0; i < numbers_siz; i++)
                {
                    const char n = numbers[i];
                    if (c == n)
                        {
                            valid = true;
                            break;
                        }
                }

            if (!valid)
                return c;
        }

    return 0;
}

void
log_confirmation_error (const dpp::confirmation_callback_t &e,
                        const char *callee)
{
    std::cerr << callee << ':' << '\n';

    dpp::error_info ev = e.get_error ();
    for (auto eve : ev.errors)
        {
            std::cerr << eve.code << '\n';
            std::cerr << eve.field << '\n';
            std::cerr << eve.reason << '\n';
            std::cerr << eve.object << '\n';
        }

    std::cerr << ev.code << '\n';
    std::cerr << ev.message << '\n';
    std::cerr << ev.human_readable << '\n';
}

} // musicat::util
