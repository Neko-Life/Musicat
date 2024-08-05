#ifndef MUSICAT_COMMAND_PAUSE_H
#define MUSICAT_COMMAND_PAUSE_H

#include <dpp/dpp.h>

namespace musicat::command::pause
{
dpp::slashcommand get_register_obj (const dpp::snowflake &sha_id);
void slash_run (const dpp::slashcommand_t &event);

void button_run (const dpp::button_click_t &event);

} // musicat::command::pause

#endif // MUSICAT_COMMAND_PAUSE_H
