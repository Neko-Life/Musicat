#include "musicat/cmds/filters.h"
#include "musicat/cmds.h"

// status, save, load, delete filters manage subcommand
namespace musicat::command::filters
{
static inline constexpr const command_handlers_map_t subcommand_handlers
    = { { "equalizer", equalizer::slash_run }, { NULL, NULL } };

dpp::slashcommand
get_register_obj (const dpp::snowflake &sha_id)
{
    dpp::slashcommand slash ("filters", "Playback filters", sha_id);

    // setup command
    equalizer::setup_subcommand (slash);

    return slash;
}

int
perquisite (const dpp::slashcommand_t &event, filters_perquisite_t *fpt)
{
    // perquisite
    player::player_manager_ptr_t player_manager = get_player_manager_ptr ();

    if (!player_manager)
        {
            return 1;
        }

    fpt->player_manager = player_manager;

    if (!player_manager->voice_ready (event.command.guild_id, event.from,
                                      event.command.usr.id))
        {
            event.reply ("Please wait while I'm getting ready to stream");
            return 1;
        }

    auto player = player_manager->get_player (event.command.guild_id);

    if (!player)
        {
            event.reply ("No active player in this guild");
            return 1;
        }

    fpt->guild_player = player;

    return 0;
}

void
slash_run (const dpp::slashcommand_t &event)
{
    auto inter = event.command.get_command_interaction ();

    if (inter.options.begin () == inter.options.end ())
        {
            fprintf (stderr, "[command::filters::slash_run ERROR] !!! No "
                             "options for command 'filters' !!!\n");

            event.reply ("I don't have that command, how'd you get that?? "
                         "Please report this to my developer");

            return;
        }

    const std::string cmd = inter.options.at (0).name;

    // run subcommand
    handle_command ({ cmd, subcommand_handlers, event });
}

} // musicat::command::filters
