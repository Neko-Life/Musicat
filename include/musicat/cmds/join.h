#ifndef MUSICAT_COMMAND_JOIN_H
#define MUSICAT_COMMAND_JOIN_H

#include <dpp/dpp.h>

namespace musicat::command::join
{
dpp::slashcommand get_register_obj (const dpp::snowflake &sha_id);

int run (const dpp::slashcommand_t &event, std::string &out);

void slash_run (const dpp::slashcommand_t &event);
} // musicat::command::join

#endif // MUSICAT_COMMAND_JOIN_H
