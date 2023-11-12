#include "musicat/cmds/filters.h"
#include "musicat/musicat.h"
#include <dpp/dpp.h>

namespace musicat::command::filters::earwax
{

void
setup_subcommand (dpp::slashcommand &slash)
{
    dpp::command_option subcmd (
        dpp::co_sub_command, "earwax",
        "Ease listening experience[ for headphone users]");

    slash.add_option (subcmd);
}

void
slash_run (const dpp::slashcommand_t &event)
{
    filters_perquisite_t ftp;

    if (perquisite (event, &ftp))
        return;

    ftp.guild_player->set_earwax = !ftp.guild_player->earwax;

    event.reply (
        std::string (ftp.guild_player->set_earwax ? "Enabling" : "Disabling")
        + " earwax");
}

} // musicat::command::filters::resample
