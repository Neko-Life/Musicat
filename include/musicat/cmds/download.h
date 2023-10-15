#ifndef MUSICAT_COMMAND_DOWNLOAD_H
#define MUSICAT_COMMAND_DOWNLOAD_H

#include "musicat/player.h"
#include <dpp/dpp.h>

namespace musicat::command::download
{
namespace autocomplete
{
void track (const dpp::autocomplete_t &event, std::string param);
}

dpp::slashcommand get_register_obj (const dpp::snowflake &sha_id);
void slash_run (const dpp::slashcommand_t &event);
} // musicat::command::download

#endif // MUSICAT_COMMAND_DOWNLOAD_H
