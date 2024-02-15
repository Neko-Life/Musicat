#include "musicat/cmds/owner.h"
#include "musicat/cmds.h"
#include "musicat/util.h"

namespace musicat::command::owner
{

dpp::slashcommand
get_register_obj (const dpp::snowflake &sha_id)
{
    dpp::slashcommand slash (
        "owner", "!!!ATTENTION!!! DO NOT USE!! WILL CAUSE SEGSVAULT!!!!!",
        sha_id);

    // setup command
    set_avatar::setup_subcommand (slash);

    return slash;
}

static inline constexpr const command_handlers_map_t subcommand_handlers
    = { { "set_avatar", set_avatar::slash_run }, { NULL, NULL } };

void
slash_run (const dpp::slashcommand_t &event)
{
    if (!is_musicat_admin (event.command.usr.id))
        {
            return event.reply (util::rand_item<std::string> (
                { "Would love to once you become my boss",
                  "Unfortunately, you haven't been granted the "
                  "privilege to make me do that",
                  "Sorryy buut no",
                  "Can't",
                  "`[ERROR]` Unable to listen to your command",
                  "uhm, who are you again?",
                  "no",
                  "Why does this command interest you?",
                  "Please wait for the command to run...",
                  "Oops unlucky, you can try again some other time",
                  "AVADACADAVRA PLZ HAPPEN 🪄 🪄",
                  "Lemme check my \"Should Obey!\" list "
                  "once again before I do that",
                  "Give me some cake first maybe then I will consider",
                  "Nothin ✨ Happened ✨✨",
                  "YOu're not my dad",
                  "I have other important stuff to do, sry",
                  "but why?",
                  "why should i?",
                  "No can do",
                  "Not a can" }));
        }

    auto inter = event.command.get_command_interaction ();

    if (inter.options.begin () == inter.options.end ())
        {
            fprintf (stderr, "[command::owner::slash_run ERROR] !!! No "
                             "options for command 'owner' !!!\n");

            event.reply ("I don't have that command, how'd you get that?? "
                         "Please report this to my developer");

            return;
        }

    const std::string cmd = inter.options.at (0).name;

    handle_command ({ cmd, subcommand_handlers, event });
}
} // musicat::command::owner
