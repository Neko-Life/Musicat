#ifndef MUSICAT_COMMAND_MOD_H
#define MUSICAT_COMMAND_MOD_H

#include <dpp/dpp.h>

namespace musicat::command::mod
{

dpp::slashcommand get_register_obj (const dpp::snowflake &sha_id);

void slash_run (const dpp::slashcommand_t &event);

namespace kick
{

void setup_subcommand (dpp::slashcommand &slash);

void slash_run (const dpp::slashcommand_t &event);

} // kick

} // musicat::command::mod

#endif // MUSICAT_COMMAND_MOD_H
