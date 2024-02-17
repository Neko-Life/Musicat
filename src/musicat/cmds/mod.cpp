#include "musicat/cmds/mod.h"
#include "musicat/cmds.h"

namespace musicat::command::mod
{

dpp::slashcommand
get_register_obj (const dpp::snowflake &sha_id)
{
    dpp::slashcommand slash ("mod", "Moderation commands", sha_id);

    // setup command
    kick::setup_subcommand (slash);

    return slash;
}

static inline constexpr const command_handlers_map_t subcommand_handlers
    = { { "kick", kick::slash_run }, { NULL, NULL } };

void
slash_run (const dpp::slashcommand_t &event)
{
    auto inter = event.command.get_command_interaction ();

    if (inter.options.begin () == inter.options.end ())
        {
            fprintf (stderr, "[command::mod::slash_run ERROR] !!! No "
                             "options for command 'mod' !!!\n");

            event.reply ("I don't have that command, how'd you get that?? "
                         "Please report this to my developer");

            return;
        }

    const std::string cmd = inter.options.at (0).name;

    handle_command ({ cmd, subcommand_handlers, event });
}
} // musicat::command::owner
