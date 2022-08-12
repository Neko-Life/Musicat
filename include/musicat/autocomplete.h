#ifndef MUSICAT_AUTOCOMPLETE_H
#define MUSICAT_AUTOCOMPLETE_H

#include <vector>
#include <string>
#include <dpp/dpp.h>

namespace musicat {
    namespace autocomplete {
        /**
         * @brief Filter candidate according to param, limit the result to 25
         *
         * @param candidates
         * @param param
         * @return std::vector<std::pair<std::string, std::string>>
         */
        std::vector<std::pair<std::string, std::string>>
            filter_candidates(const std::vector<std::pair<std::string, std::string>>& candidates,
                std::string param);

        /**
         * @brief Create autocomplete response
         *
         * @param avail
         * @param event
         */
        void create_response(const std::vector<std::pair<std::string, std::string>>& avail, const dpp::autocomplete_t& event);
    }
}

#endif // MUSICAT_AUTOCOMPLETE_H
