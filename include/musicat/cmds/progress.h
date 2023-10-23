#ifndef MUSICAT_COMMAND_PROGRESS_H
#define MUSICAT_COMMAND_PROGRESS_H

#include <dpp/dpp.h>

namespace musicat::command::progress
{
dpp::slashcommand get_register_obj (const dpp::snowflake &sha_id);
void slash_run (const dpp::slashcommand_t &event);

void update_progress (const dpp::button_click_t &event);
} // musicat::command::progress

#endif // MUSICAT_COMMAND_PROGRESS_H
