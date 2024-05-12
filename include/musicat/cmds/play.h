#ifndef MUSICAT_COMMAND_PLAY_H
#define MUSICAT_COMMAND_PLAY_H

#include <dpp/dpp.h>

namespace musicat::command::play
{
namespace autocomplete
{
void query (const dpp::autocomplete_t &event, const std::string &param);
} // autocomplete

dpp::slashcommand get_register_obj (const dpp::snowflake &sha_id);
void slash_run (const dpp::slashcommand_t &event);

} // musicat::command::play

#endif // MUSICAT_COMMAND_PLAY_H
