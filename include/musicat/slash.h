#ifndef SHA_SLASH_H
#define SHA_SLASH_H

#include <dpp/dpp.h>
#include <string>
#include <vector>

namespace musicat
{
namespace command
{
/**
 * @brief Get all application command object to register
 *
 * @param sha_id
 * @return std::vector<dpp::slashcommand>
 */
std::vector<dpp::slashcommand> get_all (dpp::snowflake sha_id);
} // command
} // musicat

#endif
