#ifndef MUSICAT_COMMAND_IMAGE_H
#define MUSICAT_COMMAND_IMAGE_H

#include <dpp/dpp.h>

namespace musicat::command::image
{
namespace autocomplete
{
void type (const dpp::autocomplete_t &event, std::string param);
}

dpp::slashcommand get_register_obj (const dpp::snowflake &sha_id);
void slash_run (const dpp::slashcommand_t &event);
} // musicat::command::image

#endif // MUSICAT_COMMAND_IMAGE_H
