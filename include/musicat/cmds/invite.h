#ifndef MUSICAT_COMMAND_INVITE_H
#define MUSICAT_COMMAND_INVITE_H

#include <dpp/dpp.h>

namespace musicat::command::invite
{
dpp::slashcommand get_register_obj (const dpp::snowflake &sha_id);
void slash_run (const dpp::slashcommand_t &event);
} // musicat::command::invite

#endif // MUSICAT_COMMAND_INVITE_H
