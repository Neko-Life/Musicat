#ifndef MUSICAT_COMMAND_LOOP_H
#define MUSICAT_COMMAND_LOOP_H

#include <dpp/dpp.h>

namespace musicat::command::loop
{
dpp::slashcommand get_register_obj (const dpp::snowflake &sha_id);

void slash_run (const dpp::slashcommand_t &event);

void button_modal_dialog (const dpp::button_click_t &event);

void handle_button_modal_dialog (const dpp::button_click_t &event);
} // musicat::command::loop

#endif // MUSICAT_COMMAND_LOOP_H
