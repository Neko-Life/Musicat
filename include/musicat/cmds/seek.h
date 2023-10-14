#ifndef MUSICAT_COMMAND_SEEK_H
#define MUSICAT_COMMAND_SEEK_H

#include <dpp/dpp.h>

namespace musicat::command::seek
{
dpp::slashcommand get_register_obj (const dpp::snowflake &sha_id);
void slash_run (const dpp::slashcommand_t &event);
} // musicat::command::seek

#endif // MUSICAT_COMMAND_SEEK_H
