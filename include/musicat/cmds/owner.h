#ifndef MUSICAT_COMMAND_OWNER_H
#define MUSICAT_COMMAND_OWNER_H

#include <dpp/dpp.h>

namespace musicat::command::owner
{

dpp::slashcommand get_register_obj (const dpp::snowflake &sha_id);

void slash_run (const dpp::slashcommand_t &event);

namespace set_avatar
{

void setup_subcommand (dpp::slashcommand &slash);

void slash_run (const dpp::slashcommand_t &event);

} // set_avatar

namespace set_presence
{

void setup_subcommand (dpp::slashcommand &slash);

void slash_run (const dpp::slashcommand_t &event);

} // set_presence

namespace system
{

void setup_subcommand (dpp::slashcommand &slash);

void slash_run (const dpp::slashcommand_t &event);

} // system

} // musicat::command::owner

#endif // MUSICAT_COMMAND_OWNER_H
