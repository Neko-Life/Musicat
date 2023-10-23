#ifndef MUSICAT_COMMAND_SEARCH_H
#define MUSICAT_COMMAND_SEARCH_H

#include <dpp/dpp.h>

namespace musicat::command::search
{
dpp::slashcommand get_register_obj (const dpp::snowflake &sha_id);

dpp::interaction_modal_response modal_enqueue_searched_track ();
dpp::interaction_modal_response modal_enqueue_searched_track_top ();
dpp::interaction_modal_response modal_enqueue_searched_track_slip ();

void slash_run (const dpp::slashcommand_t &event);
} // musicat::command::search

#endif // MUSICAT_COMMAND_SEARCH_H
