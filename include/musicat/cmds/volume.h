#ifndef MUSICAT_COMMAND_VOLUME_H
#define MUSICAT_COMMAND_VOLUME_H

#include <dpp/dpp.h>

namespace musicat::command::volume
{
dpp::slashcommand get_register_obj (const dpp::snowflake &sha_id);
void slash_run (const dpp::slashcommand_t &event);
} // musicat::command::volume

#endif // MUSICAT_COMMAND_VOLUME_H
