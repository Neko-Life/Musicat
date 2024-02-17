#include "musicat/cmds/mod.h"

namespace musicat::command::mod::kick
{
void
setup_subcommand (dpp::slashcommand &slash)
{
    dpp::command_option subcmd (dpp::co_sub_command, "kick",
                                "Kick user[ from your server]");

    subcmd.add_option (
        dpp::command_option (dpp::co_user, "user", "User[ to kick]", true));

    slash.add_option (subcmd);
}

void
slash_run (const dpp::slashcommand_t &event)
{
    // !TODO
}
} // musicat::command::mod::kick
