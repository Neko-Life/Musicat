#ifndef SHA_STORAGE_H
#define SHA_STORAGE_H

#include <map>
#include <any>
#include <dpp/dpp.h>

namespace musicat {

    /**
     * @brief Currently only for messages, will need refactor to add storage for other types
     *
     */
    namespace storage {

        /**
         * @brief Temporary data for messages container with any type, with message_id as key.
         * MUST be controlled (eg. delete when message is deleted) and keep in check by gcs.
         */
        extern std::map<uint64_t, std::any> messages_storage;

        std::any get(uint64_t id);
        std::pair<std::map<uint64_t, std::any>::iterator, bool> set(uint64_t id, std::any data);

        /**
         * @brief Erase data from storage
         *
         * @param id
         * @return std::size_t Number of data erased
         */
        std::size_t remove(uint64_t id);

        /**
         * @brief Get storage size
         *
         * @return std::size_t
         */
        std::size_t size();
    }
}

#endif
