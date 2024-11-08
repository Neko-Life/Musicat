#include "musicat/autocomplete.h"
#include "musicat/util.h"
#include <dpp/dpp.h>

namespace musicat::autocomplete
{

constexpr size_t max_item_count = 25U;

std::vector<std::pair<std::string, std::string> >
filter_candidates (
    const std::vector<std::pair<std::string, std::string> > &candidates,
    std::string param, const bool fuzzy)
{
    std::vector<std::pair<std::string, std::string> > avail = {};

    avail.reserve (max_item_count);

    // trim param cuz discord wants trimmed string
    param = util::trim_str (param);

    bool param_empty = param.empty ();

#ifdef AC_INCLUDE_INP
    if (!param_empty)
        avail.push_back (std::make_pair (param, param));

    size_t siz = param_empty ? 0 : 1;
#else
    size_t siz = 0;
#endif // AC_INCLUDE_INP

    if (param_empty)
        {
            auto cbegin = candidates.begin ();
            size_t cmp = max_item_count > siz ? max_item_count - siz : 0;

            if (cmp > 0U)
                {
                    avail.insert (avail.begin (), cbegin,
                                  candidates.size () > cmp
                                      ? cbegin + cmp
                                      : candidates.end ());
                }
        }
    else
        for (const std::pair<std::string, std::string> &i : candidates)
            {
                std::string ori = i.first;
                ori = util::trim_str (ori);
                if (ori.empty ())
                    ori = "[NULL]";

                // whether this candidate is a match
                bool is_match = false;

                if (fuzzy)
                    // use fuzzy search algorithm
                    {
                        if (util::fuzzy_match (param, i.first, true))
                            is_match = true;
                    }
                // default algorithm
                else
                    {
                        // !TODO: move this to util::basic_match (search, str,
                        // case_insensitive)
                        std::string i2 = i.first;
                        for (char &j : i2)
                            j = std::tolower (j);
                        for (char &j : param)
                            j = std::tolower (j);
                        if (i2.find (param) != std::string::npos)
                            is_match = true;
                    }

                if (!is_match)
                    continue;

                avail.push_back (std::make_pair (ori, i.second));
                siz++;

                if (siz >= max_item_count)
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

    const bool b_25 = avail.size () > max_item_count;
    int siz = 0;

    for (const std::pair<std::string, std::string> &i : avail)
        {
            std::string v;
            std::string v2;

            util::u8_limit_length (i.first.c_str (), v);
            util::u8_limit_length (i.second.c_str (), v2);

            r.add_autocomplete_choice (dpp::command_option_choice (v, v2));
            if (b_25 && (++siz) >= 25)
                break;
        }
    event.from->creator->interaction_response_create (event.command.id,
                                                      event.command.token, r);
}
} // namespace musicat::autocomplete
