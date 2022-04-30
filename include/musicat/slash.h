#ifndef SHA_SLASH_H
#define SHA_SLASH_H

#include <dpp/dpp.h>
#include <string>
#include <vector>

using string = std::string;

namespace sha_slash {
    std::vector<dpp::slashcommand> get_all(dpp::snowflake sha_id);
}

#endif
