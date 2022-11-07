#include <dpp/dpp.h>
#include "musicat/autocomplete.h"

namespace musicat {
    namespace autocomplete {
        std::vector<std::pair<std::string, std::string>>
            filter_candidates(const std::vector<std::pair<std::string, std::string>>& candidates, std::string param) {

            std::vector<std::pair<std::string, std::string>> avail = {};
            avail.reserve(25U);

            for (const std::pair<std::string, std::string>& i : candidates)
            {
                const std::string ori = i.first;
                std::string i2 = i.first;
                for (char& j : i2) j = std::tolower(j);
                for (char& j : param) j = std::tolower(j);
                if (i2.find(param) != std::string::npos)
                    avail.push_back(std::make_pair(ori, i.second));
                if (avail.size() == 25U) break;
            }

            return avail;
        }

        void create_response(const std::vector<std::pair<std::string, std::string>>& avail, const dpp::autocomplete_t& event) {
            dpp::interaction_response r(dpp::ir_autocomplete_reply);

            const bool b_25 = avail.size() > 25U;

            for (const std::pair<std::string, std::string>& i : avail)
            {
                std::string v = i.first.length() > 95 ? i.first.substr(0, 95) : i.first;
                std::string v2 = i.second.length() > 95 ? i.second.substr(0, 95) : i.second;
                r.add_autocomplete_choice(dpp::command_option_choice(v, v2));
                if (b_25 && r.autocomplete_choices.size() == 25U) break;
            }
            event.from->creator->interaction_response_create(event.command.id, event.command.token, r);
        }
    }
}
