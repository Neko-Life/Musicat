#ifndef MUSICAT_COMMAND_BUBBLE_WRAP_H
#define MUSICAT_COMMAND_BUBBLE_WRAP_H

#include <dpp/dpp.h>

namespace musicat::command::bubble_wrap
{
dpp::slashcommand get_register_obj (const dpp::snowflake &sha_id);
void slash_run (const dpp::slashcommand_t &event);
} // musicat::command::bubble_wrap

#endif // MUSICAT_COMMAND_BUBBLE_WRAP_H
