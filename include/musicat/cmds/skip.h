#ifndef MUSICAT_COMMAND_SKIP_H
#define MUSICAT_COMMAND_SKIP_H

#include <dpp/dpp.h>

namespace musicat::command::skip
{
dpp::slashcommand get_register_obj (const dpp::snowflake &sha_id);
void slash_run (const dpp::slashcommand_t &event);
} // musicat::command::skip

#endif // MUSICAT_COMMAND_SKIP_H
