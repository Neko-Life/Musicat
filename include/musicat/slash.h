#ifndef SHA_SLASH_H
#define SHA_SLASH_H

#include <dpp/dpp.h>
#include <string>
#include <vector>

namespace musicat_slash {
    using string = std::string;

    enum queue_modify_t : int8_t {
        // Shuffle the queue
        m_shuffle = 0,
        // Reverse the queue
        m_reverse = 1,
        // Clear songs added by users who left the vc
        m_clear_left = 3,
        // Clear queue
        m_clear = 2
    };

    std::vector<dpp::slashcommand> get_all(dpp::snowflake sha_id);
}

#endif
