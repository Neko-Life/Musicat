#ifndef SHA_STORAGE_H
#define SHA_STORAGE_H

#include <any>
#include <dpp/dpp.h>
#include <map>

namespace musicat
{

/**
 * @brief Currently only for messages, will need refactor to add storage for
 * other types
 *
 */
namespace storage
{

/**
 * @brief Temporary data for messages container with any type, with message_id
 * as key. MUST be controlled (eg. delete when message is deleted) and keep in
 * check by gcs.
 */
extern std::map<dpp::snowflake, std::any> messages_storage;

std::any get (dpp::snowflake id);
std::pair<std::map<dpp::snowflake, std::any>::iterator, bool>
set (dpp::snowflake id, std::any data);

/**
 * @brief Erase data from storage
 *
 * @param id
 * @return std::size_t Number of data erased
 */
std::size_t remove (dpp::snowflake id);

/**
 * @brief Get storage size
 *
 * @return std::size_t
 */
std::size_t size ();
}
}

#endif
