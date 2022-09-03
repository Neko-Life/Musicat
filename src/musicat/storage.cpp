#include "musicat/storage.h"
#include <dpp/snowflake.h>

namespace musicat {
    namespace storage {
        std::map<dpp::snowflake, std::any> messages_storage = {};

        std::any get(dpp::snowflake id) {
            auto ret = messages_storage.find(id);
            if (ret == messages_storage.end()) return std::any();
            return ret->second;
        }

        std::pair<std::map<dpp::snowflake, std::any>::iterator, bool> set(dpp::snowflake id, std::any data) {
            return messages_storage.insert_or_assign(id, data);
        }

        std::size_t remove(dpp::snowflake id) {
            return messages_storage.erase(id);
        }

        std::size_t size() {
            return messages_storage.size();
        }
    }
}
