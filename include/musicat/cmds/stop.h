#ifndef MUSICAT_COMMAND_STOP_H
#define MUSICAT_COMMAND_STOP_H

#include <dpp/dpp.h>

namespace musicat::command::stop
{
dpp::slashcommand get_register_obj (const dpp::snowflake &sha_id);
void slash_run (const dpp::slashcommand_t &event);
} // musicat::command::stop

#endif // MUSICAT_COMMAND_STOP_H
