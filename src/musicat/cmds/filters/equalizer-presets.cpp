#include "musicat/cmds.h"
#include "musicat/cmds/filters.h"

namespace musicat::command::filters::equalizer_presets
{

void
setup_subcommand (dpp::slashcommand &slash)
{
    dpp::command_option subcmd (dpp::co_sub_command, "equalizer_presets",
                                "Equalizer presets");

    subcmd.add_option (
        dpp::command_option (dpp::co_sub_command, "save",
                             "Save current active equalizer settings")
            .add_option (dpp::command_option (dpp::co_string, "name",
                                              "Preset name", true)));

    subcmd.add_option (
        dpp::command_option (dpp::co_sub_command, "load", "Load saved preset")
            .add_option (dpp::command_option (dpp::co_string, "name",
                                              "Preset name", true)
                             .set_auto_complete (true)));

    subcmd.add_option (
        dpp::command_option (dpp::co_sub_command, "view",
                             "View saved preset settings")
            .add_option (dpp::command_option (dpp::co_string, "name",
                                              "Preset name", true)
                             .set_auto_complete (true)));

    slash.add_option (subcmd);
}

void
save (const dpp::slashcommand_t &event)
{
    auto player_manager = get_player_manager_ptr ();
    if (!player_manager)
        {
            return;
        }

    auto guild_player = player_manager->create_player (event.command.guild_id);
    if (!guild_player)
        return event.reply ("`[ERROR]` Failed creating guild player");

    if (guild_player->equalizer.empty ())
        return event.reply ("Equalizer not set");

    std::string name;
    get_inter_param (event, "name", &name);


    // !TODO
}

void
load (const dpp::slashcommand_t &event)
{
}

void
view (const dpp::slashcommand_t &event)
{
}

static inline constexpr const command_handlers_map_t subcommand_handlers
    = { { "save", save }, { "load", load }, { "view", view }, { NULL, NULL } };

void
slash_run (const dpp::slashcommand_t &event)
{
    auto inter = event.command.get_command_interaction ();

    if (inter.options.begin () == inter.options.end ())
        {
            fprintf (stderr,
                     "[command::filters::equalizer_presets::slash_run ERROR] "
                     "!!! No options for command 'filters equalizer_presets' "
                     "!!!\n");

            event.reply ("I don't have that command, how'd you get that?? "
                         "Please report this to my developer");

            return;
        }

    const std::string cmd = inter.options.at (0).name;

    std::cout << "equalizer_presets::slash_run: cmd:" << cmd << '\n';

    // run subcommand
    // handle_command ({ cmd, subcommand_handlers, event });
}

} // musicat::command::filters::equalizer_presets
