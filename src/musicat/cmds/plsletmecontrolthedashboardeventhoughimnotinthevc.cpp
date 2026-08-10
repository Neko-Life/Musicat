#include "musicat/player.h"

#include "musicat/cmds.h"
#include "musicat/cmds/plsletmecontrolthedashboardeventhoughimnotinthevc.h"

namespace musicat::command::plsletmecontrolthedashboardeventhoughimnotinthevc
{
dpp::slashcommand
get_register_obj (const dpp::snowflake &sha_id)
{
    return dpp::slashcommand ("plsletmecontrolthedashboardeventhoughimnotinthevc", "Self descripted", sha_id);
}

void
slash_run (const dpp::slashcommand_t &event)
{
    if (!cmd_pre_get_player_manager_ready (event))
        return;

    auto guild_player = player::manager::get_player (event.command.guild_id);
    if (!guild_player || guild_player->current_track.is_empty ())
        return event.reply ("Nothing is playing right now, try the `/play` command");

    if (event.command.usr.id)
        {
            guild_player->dashboard_control_requests.push_back (event.command.usr.id);
            return event.reply ("k");
        }

    return event.reply ("Somethin tells me ur not worth it");
}

} // musicat::command::plsletmecontrolthedashboardeventhoughimnotinthevc
