#ifndef MUSICAT_COMMAND_LEAVE_H
#define MUSICAT_COMMAND_LEAVE_H

#include <dpp/dpp.h>

namespace musicat::command::leave
{
dpp::slashcommand get_register_obj (const dpp::snowflake &sha_id);
void slash_run (const dpp::slashcommand_t &event);
} // musicat::command::leave

#endif // MUSICAT_COMMAND_LEAVE_H
