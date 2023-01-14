#include "musicat/cmds.h"

namespace musicat
{
namespace command
{
namespace download
{
namespace autocomplete
{
void
track (const dpp::autocomplete_t &event, std::string param,
       player::player_manager_ptr player_manager)
{
    play::autocomplete::query(event, param, player_manager);
}
} // autocomplete

dpp::slashcommand
get_register_obj (const dpp::snowflake &sha_id)
{
    return dpp::slashcommand ("download", "Download [a track]", sha_id)
        .add_option (dpp::command_option (dpp::co_string, "track",
                                          "Track [to download]")
                         .set_auto_complete (true));
}

void
slash_run (const dpp::interaction_create_t &event)
{
    std::string filename = "";
    get_inter_param (event, "track", &filename);

    if (!filename.length ())
        return event.reply ("Provide `track`");

    event.thinking();
}
} // download
} // command
} // musicat
