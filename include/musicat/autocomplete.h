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
            filter_candidates(std::vector<std::pair<std::string, std::string>> candidates,
                std::string param);

        /**
         * @brief Create event autocomplete response
         *
         * @param avail
         * @param client
         * @param command_id
         * @param command_token
         */
        void create_response(std::vector<std::pair<std::string, std::string>> avail,
            dpp::cluster& client,
            dpp::snowflake command_id,
            std::string command_token);
    }
}

#endif // MUSICAT_AUTOCOMPLETE_H
