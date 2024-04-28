#include "musicat/cmds/filters.h"
#include "musicat/musicat.h"

namespace musicat::command::filters::list_filters
{

void
setup_subcommand (dpp::slashcommand &slash)
{
    dpp::command_option subcmd (dpp::co_sub_command, "list_filters",
                                "List[ active] filter[s]");

    slash.add_option (subcmd);
}

void
slash_run (const dpp::slashcommand_t &event)
{
    filters_perquisite_t ftp;

    if (perquisite (event, &ftp))
        return;

    // const std::string msg = ftp.guild_player->fx_get_all_info_str ();

    // if (msg.empty ())
    //     return event.reply ("No active filter");

    // event.reply (msg);
}

} // musicat::command::filters::list_filters
