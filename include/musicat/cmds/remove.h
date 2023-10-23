#ifndef MUSICAT_COMMAND_REMOVE_H
#define MUSICAT_COMMAND_REMOVE_H

#include <dpp/dpp.h>

namespace musicat::command::remove
{
dpp::slashcommand get_register_obj (const dpp::snowflake &sha_id);
void slash_run (const dpp::slashcommand_t &event);
} // musicat::command::remove

#endif // MUSICAT_COMMAND_REMOVE_H
