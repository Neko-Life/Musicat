#include "musicat/storage.h"

namespace musicat {
    namespace storage {
        std::map<uint64_t, std::any> messages_storage = {};

        std::any get(uint64_t id) {
            auto ret = messages_storage.find(id);
            if (ret == messages_storage.end()) return std::any();
            return ret->second;
        }

        std::pair<std::map<uint64_t, std::any>::iterator, bool> set(uint64_t id, std::any data) {
            return messages_storage.insert_or_assign(id, data);
        }

        std::size_t remove(uint64_t id) {
            return messages_storage.erase(id);
        }

        std::size_t size() {
            return messages_storage.size();
        }
    }
}
