#ifndef MUSICAT_COMMAND_AUTOPLAY_H
#define MUSICAT_COMMAND_AUTOPLAY_H

#include <dpp/dpp.h>

namespace musicat::command::autoplay
{
dpp::slashcommand get_register_obj (const dpp::snowflake &sha_id);
void slash_run (const dpp::slashcommand_t &event);
} // musicat::command::autoplay

#endif // MUSICAT_COMMAND_AUTOPLAY_H
