#ifndef SHA_SLASH_H
#define SHA_SLASH_H

#include <dpp/dpp.h>
#include <string>
#include <vector>

namespace musicat_slash {
    using string = std::string;

    std::vector<dpp::slashcommand> get_all(dpp::snowflake sha_id);
}

#endif
