#include "appcommand.h"
#include "musicat/autocomplete.h"
#include "musicat/cmds.h"
#include "musicat/cmds/filters.h"
#include "musicat/db.h"
#include "musicat/musicat.h"
#include "musicat/thread_manager.h"
#include <libpq-fe.h>

namespace musicat::command::filters::equalizer_presets
{

namespace autocomplete
{
void
name (const dpp::autocomplete_t &event, const std::string &param)
{
    std::pair<PGresult *, ExecStatusType> res
        = database::get_all_equalizer_preset_name ();

    std::vector<std::pair<std::string, std::string> > response = {};

    if (res.second == PGRES_TUPLES_OK)
        {
            int rn = 0;
            while (true)
                {
                    if (PQgetisnull (res.first, rn, 0))
                        break;

                    std::string val
                        = std::string (PQgetvalue (res.first, rn, 0));

                    response.push_back (std::make_pair (val, val));
                    rn++;
                }
        }

    database::finish_res (res.first);
    res.first = nullptr;

    musicat::autocomplete::create_response (
        musicat::autocomplete::filter_candidates (response, param), event);
}

} // autocomplete

void
setup_subcommand (dpp::slashcommand &slash)
{
    dpp::command_option subcmd (dpp::co_sub_command_group, "equalizer_presets",
                                "Equalizer presets");

    subcmd.add_option (
        dpp::command_option (dpp::co_sub_command, "save",
                             "Save current active equalizer settings")
            .add_option (dpp::command_option (dpp::co_string, "name",
                                              "Preset name", true)
                             .set_max_length (100)));

    subcmd.add_option (
        dpp::command_option (dpp::co_sub_command, "load", "Load saved preset")
            .add_option (dpp::command_option (dpp::co_string, "name",
                                              "Preset name", true)
                             .set_max_length (100)
                             .set_auto_complete (true)));

    subcmd.add_option (
        dpp::command_option (dpp::co_sub_command, "view",
                             "View saved preset settings")
            .add_option (dpp::command_option (dpp::co_string, "name",
                                              "Preset name", true)
                             .set_max_length (100)
                             .set_auto_complete (true)));

    slash.add_option (subcmd);
}

void
save (const dpp::slashcommand_t &event)
{
    std::thread rt ([event] () {
        auto *player_manager = get_player_manager_ptr ();
        if (!player_manager)
            {
                return;
            }

        auto guild_player
            = player_manager->create_player (event.command.guild_id);

        if (!guild_player)
            return event.reply ("`[ERROR]` Failed creating guild player");

        if (guild_player->equalizer.empty ())
            return event.reply ("Equalizer not set");

        std::string name;
        get_inter_param (event, "name", &name);

        event.thinking ();

        ExecStatusType status = database::create_equalizer_preset (
            name, guild_player->equalizer, event.command.usr.id);

        switch (status)
            {
            case -1:
                return event.edit_response ("`[ERROR]` Missing argument");
            case -2:
                return event.edit_response (
                    "`[ERROR]` Preset name should match validation regex: "
                    "^[0-9a-zA-Z_ -]{1,100}$");
            case PGRES_COMMAND_OK:
                return event.edit_response (
                    "Created new preset with name: `" + name
                    + "`\nSettings:```md\n"
                    + equalizer_fx_t_to_slash_args (
                        af_args_to_equalizer_fx_t (guild_player->equalizer))
                    + "```");
            default:
                return event.edit_response ("Preset with that name already "
                                            "exist! Try pick another name");
            }
    });

    thread_manager::dispatch (rt);
}

void
load_or_view (const dpp::slashcommand_t &event, bool is_view = false)
{
    std::thread rt ([event, is_view] () {
        auto *player_manager = get_player_manager_ptr ();
        if (!player_manager)
            {
                return;
            }

        auto guild_player
            = player_manager->create_player (event.command.guild_id);

        if (!guild_player)
            return event.reply ("`[ERROR]` Failed creating guild player");

        std::string name;
        get_inter_param (event, "name", &name);

        event.thinking ();

        auto res = database::get_equalizer_preset (name);

        switch (res.second)
            {
            case -1:
                return event.edit_response ("`[ERROR]` Invalid name");
            }

        const std::string &val = res.first.first, ori_name = res.first.second;

        if (val.empty ())
            return event.edit_response ("Can't find preset with that name");

        if (is_view)
            {
                event.edit_response ("Preset settings: `" + ori_name
                                     + "` ```md\n"
                                     + equalizer_fx_t_to_slash_args (
                                         af_args_to_equalizer_fx_t (val))
                                     + "```");

                return;
            }

        if (guild_player->equalizer == val)
            return event.edit_response (
                "Current settings already match preset");

        guild_player->set_equalizer = val;

        event.edit_response (
            "Setting preset:```md\n"
            + equalizer_fx_t_to_slash_args (af_args_to_equalizer_fx_t (val))
            + "```");
    });

    thread_manager::dispatch (rt);
}

void
load (const dpp::slashcommand_t &event)
{
    load_or_view (event);
}

void
view (const dpp::slashcommand_t &event)
{
    load_or_view (event, true);
}

static inline constexpr const command_handlers_map_t subcommand_handlers
    = { { "save", save }, { "load", load }, { "view", view }, { NULL, NULL } };

void
slash_run (const dpp::slashcommand_t &event)
{
    auto inter = event.command.get_command_interaction ();

    auto &current_cmd = inter.options.at (0);

    if (current_cmd.options.begin () == current_cmd.options.end ())
        {
            fprintf (stderr,
                     "[command::filters::equalizer_presets::slash_run ERROR] "
                     "!!! No options for command 'filters equalizer_presets' "
                     "!!!\n");

            event.reply ("I don't have that command, how'd you get that?? "
                         "Please report this to my developer");

            return;
        }

    const std::string cmd = current_cmd.options.at (0).name;

    // run subcommand
    handle_command ({ cmd, subcommand_handlers, event });
}

} // musicat::command::filters::equalizer_presets
