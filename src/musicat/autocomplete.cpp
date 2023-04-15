#include "musicat/autocomplete.h"
#include "musicat/util.h"
#include "musicat/musicat.h"
#include <dpp/dpp.h>

namespace musicat
{
namespace autocomplete
{
std::vector<std::pair<std::string, std::string> >
filter_candidates (
    const std::vector<std::pair<std::string, std::string> > &candidates,
    std::string param, const bool fuzzy)
{
    std::vector<std::pair<std::string, std::string> > avail = {};
    avail.reserve (25U);

    for (const std::pair<std::string, std::string> &i : candidates)
        {
            const std::string ori = i.first;
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
                    // !TODO: move this to util::basic_match (search, str, case_insensitive)
                    std::string i2 = i.first;
                    for (char &j : i2)
                        j = std::tolower (j);
                    for (char &j : param)
                        j = std::tolower (j);
                    if (i2.find (param) != std::string::npos)
                        is_match = true;
                }

            if (is_match)
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
            char v[512];
            char v2[512];

            util::u8_limit_length(i.first.c_str(), v);
            util::u8_limit_length(i.second.c_str(), v2);

            r.add_autocomplete_choice (dpp::command_option_choice (v, v2));
            if (b_25 && r.autocomplete_choices.size () == 25U)
                break;
        }
    event.from->creator->interaction_response_create (event.command.id,
                                                      event.command.token, r);
}
} // namespace autocomplete
} // namespace musicat
