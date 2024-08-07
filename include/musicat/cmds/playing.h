#ifndef MUSICAT_COMMAND_PLAYING_H
#define MUSICAT_COMMAND_PLAYING_H

#include <dpp/dpp.h>

namespace musicat::command::playing
{

dpp::slashcommand get_register_obj (const dpp::snowflake &sha_id);

void slash_run (const dpp::slashcommand_t &event);

} // musicat::command::playing

#endif // MUSICAT_COMMAND_PLAYING_H
