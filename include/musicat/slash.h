#ifndef SHA_SLASH_H
#define SHA_SLASH_H

#include "musicat/cmds.h"
#include <dpp/dpp.h>
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
std::vector<dpp::slashcommand> get_all (const dpp::snowflake &sha_id);

/**
 * @brief Get slash command handlers map
 */
const command_handler_t *get_slash_command_handlers ();
} // command
} // musicat

#endif
