#ifndef MUSICAT_COMMAND_MOVE_H
#define MUSICAT_COMMAND_MOVE_H

#include <dpp/dpp.h>

namespace musicat::command::move
{
dpp::slashcommand get_register_obj (const dpp::snowflake &sha_id);
void slash_run (const dpp::slashcommand_t &event);
} // musicat::command::move

#endif // MUSICAT_COMMAND_MOVE_H
