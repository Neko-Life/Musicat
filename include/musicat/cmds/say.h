#ifndef MUSICAT_COMMAND_SAY_H
#define MUSICAT_COMMAND_SAY_H

#include <dpp/dpp.h>

namespace musicat::command::say
{
dpp::slashcommand get_register_obj (const dpp::snowflake &sha_id);
void slash_run (const dpp::slashcommand_t &event);
} // musicat::command::say

#endif // MUSICAT_COMMAND_SAY_H
