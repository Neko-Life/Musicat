#ifndef MUSICAT_COMMAND_PLAYLIST_H
#define MUSICAT_COMMAND_PLAYLIST_H

#include <dpp/dpp.h>

namespace musicat::command::playlist
{
namespace autocomplete
{
void id (const dpp::autocomplete_t &event, std::string param);
} // autocomplete

namespace save
{
dpp::command_option get_option_obj ();
void slash_run (const dpp::slashcommand_t &event);
} // save

namespace load
{
dpp::command_option get_option_obj ();
void slash_run (const dpp::slashcommand_t &event);
} // load

namespace view
{
dpp::command_option get_option_obj ();
void slash_run (const dpp::slashcommand_t &event);
} // view

namespace delete_
{
dpp::command_option get_option_obj ();
void slash_run (const dpp::slashcommand_t &event);
} // delete_

dpp::slashcommand get_register_obj (const dpp::snowflake &sha_id);
void slash_run (const dpp::slashcommand_t &event);
} // musicat::command::playlist

#endif // MUSICAT_COMMAND_PLAYLIST_H
