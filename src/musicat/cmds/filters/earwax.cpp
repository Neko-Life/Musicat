#include "musicat/cmds/filters.h"
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

    // !TODO: ffmpeg earwax fx default sample rate is 44.1KHz, add
    // sampling_rate argument and make the default to 48KHz
    ftp.guild_player->earwax = !ftp.guild_player->earwax;
    ftp.guild_player->set_earwax = true;

    event.reply (
        std::string (ftp.guild_player->earwax ? "Enabling" : "Disabling")
        + " earwax");
}

} // musicat::command::filters::resample
