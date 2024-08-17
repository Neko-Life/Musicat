#ifndef MUSICAT_COMMAND_SEEK_H
#define MUSICAT_COMMAND_SEEK_H

#include <dpp/dpp.h>

namespace musicat::command::seek
{
dpp::slashcommand get_register_obj (const dpp::snowflake &sha_id);
void slash_run (const dpp::slashcommand_t &event);

void button_run_rewind (const dpp::button_click_t &event);
int button_seek_zero (const dpp::button_click_t &event);
void button_run_forward (const dpp::button_click_t &event);

} // musicat::command::seek

#endif // MUSICAT_COMMAND_SEEK_H
