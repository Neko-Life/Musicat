#ifndef MUSICAT_COMMAND_HELLO_H
#define MUSICAT_COMMAND_HELLO_H

#include <dpp/dpp.h>

namespace musicat::command::hello
{
dpp::slashcommand get_register_obj (const dpp::snowflake &sha_id);
void slash_run (const dpp::slashcommand_t &event);
} // musicat::command::hello

#endif // MUSICAT_COMMAND_HELLO_H
