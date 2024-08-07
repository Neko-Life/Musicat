/*
Apply 18 band equalizer.

The filter accepts the following options:

1b

    Set 65Hz band gain.
2b

    Set 92Hz band gain.
3b

    Set 131Hz band gain.
4b

    Set 185Hz band gain.
5b

    Set 262Hz band gain.
6b

    Set 370Hz band gain.
7b

    Set 523Hz band gain.
8b

    Set 740Hz band gain.
9b

    Set 1047Hz band gain.
10b

    Set 1480Hz band gain.
11b

    Set 2093Hz band gain.
12b

    Set 2960Hz band gain.
13b

    Set 4186Hz band gain.
14b

    Set 5920Hz band gain.
15b

    Set 8372Hz band gain.
16b

    Set 11840Hz band gain.
17b

    Set 16744Hz band gain.
18b

    Set 20000Hz band gain.

action: set balance reset
up the volume threshold to 500
*/

#include "musicat/cmds.h"
#include "musicat/cmds/filters.h"
#include "musicat/musicat.h"
#include "musicat/util.h"

namespace musicat::command::filters::equalizer
{

inline constexpr const struct
{
    const char *set = "0";
    const char *balance = "1";
    const char *reset = "2";
} action_t_e;

inline constexpr const char *eq_options[][2] = {
    { "band-1", "Set 65Hz band gain" },
    { "band-2", "Set 92Hz band gain" },
    { "band-3", "Set 131Hz band gain" },
    { "band-4", "Set 185Hz band gain" },
    { "band-5", "Set 262Hz band gain" },
    { "band-6", "Set 370Hz band gain" },
    { "band-7", "Set 523Hz band gain" },
    { "band-8", "Set 740Hz band gain" },
    { "band-9", "Set 1047Hz band gain" },
    { "volume", "Set filter volume" },
    { "band-10", "Set 1480Hz band gain" },
    { "band-11", "Set 2093Hz band gain" },
    { "band-12", "Set 2960Hz band gain" },
    { "band-13", "Set 4186Hz band gain" },
    { "band-14", "Set 5920Hz band gain" },
    { "band-15", "Set 8372Hz band gain" },
    { "band-16", "Set 11840Hz band gain" },
    { "band-17", "Set 16744Hz band gain" },
    { "band-18", "Set 20000Hz band gain" },
};

inline constexpr const size_t eq_options_size
    = (sizeof (eq_options) / sizeof (*eq_options));

inline constexpr const int min_num_opt_val = 1;
inline constexpr const int max_num_opt_val = 200;

void
setup_subcommand (dpp::slashcommand &slash)
{
    // this code was made under the impression that slash command can't have
    // 18+ arguments and so implemented in the code to "divide" the arguments
    // into few command, which adjusted after the above was proved to be false,
    // refactor this sometime
    constexpr int argpc = eq_options_size;

    constexpr size_t igoal = (eq_options_size / argpc);
    for (size_t i = 0; i < igoal; i++)
        {
            dpp::command_option eqsubcmd (dpp::co_sub_command, "equalizer",
                                          "Apply 18 band equalizer");

            eqsubcmd.add_option (
                dpp::command_option (dpp::co_string, "action",
                                     "What you wanna do?", false)
                    .add_choice (
                        dpp::command_option_choice ("Set", action_t_e.set))
                    .add_choice (dpp::command_option_choice (
                        "Balance", action_t_e.balance))
                    .add_choice (dpp::command_option_choice (
                        "Reset", action_t_e.reset)));

            size_t jgoal = (i + 1) * argpc;
            for (size_t j = i * argpc; j < jgoal && j < eq_options_size; j++)
                {
                    eqsubcmd.add_option (
                        dpp::command_option (dpp::co_integer, eq_options[j][0],
                                             eq_options[j][1], false)
                            .set_min_value (min_num_opt_val)
                            .set_max_value (max_num_opt_val));
                }

            slash.add_option (eqsubcmd);
        }
}

inline constexpr const command_handlers_map_t action_handlers
    = { { "", show },
        { action_t_e.set, set },
        { action_t_e.balance, balance },
        { action_t_e.reset, reset },
        { NULL, NULL } };

void
show (const dpp::slashcommand_t &event)
{
    filters_perquisite_t ftp;

    if (perquisite (event, &ftp))
        return;

    if (ftp.guild_player->equalizer.empty ())
        return event.reply ("Equalizer not set");

    std::string rply
        = "This command is still under construction...\n\n"
          "Here's what you want anyway: ```md\n"
          + equalizer_fx_t_to_slash_args (
              af_args_to_equalizer_fx_t (ftp.guild_player->equalizer))
          + "```";

    event.reply (rply);
}

void
set (const dpp::slashcommand_t &event)
{
    filters_perquisite_t ftp;

    if (perquisite (event, &ftp))
        return;

    // !TODO: parse current setting to preserve undefined arg value
    equalizer_fx_t arg
        = ftp.guild_player->equalizer.empty ()
              ? create_equalizer_fx_t ()
              : af_args_to_equalizer_fx_t (ftp.guild_player->equalizer);

    get_inter_param (event, "volume", &arg.volume);

    for (int i = 0; i < 18; i++)
        {
            get_inter_param (event, "band-" + std::to_string (i + 1),
                             (arg.bands + i));
        }

    std::string set_str = equalizer_fx_t_to_af_args (arg);

    ftp.guild_player->equalizer = set_str;
    ftp.guild_player->set_equalizer = true;

    std::string rply = "Setting equalizer with args: ```md\n"
                       + equalizer_fx_t_to_slash_args (arg) + "```";

    event.reply (rply);
}

void
balance (const dpp::slashcommand_t &event)
{
    filters_perquisite_t ftp;

    if (perquisite (event, &ftp))
        return;

    constexpr const char *new_equalizer
        = "1b=0.5:2b=0.5:3b=0.5:4b=0.5:5b=0.5:6b=0.5:7b=0.5:8b="
          "0.5:9b=0.5:10b=0.5:11b=0.5:12b=0.5:13b=0.5:14b=0.5:15b=0.5:16b=0.5:"
          "17b=0.5:18b=0.5,volume=1"; // volume of 1 is 100%

    ftp.guild_player->equalizer = new_equalizer;
    ftp.guild_player->set_equalizer = true;

    event.reply ("Balancing...");
}

void
reset (const dpp::slashcommand_t &event)
{
    filters_perquisite_t ftp;

    if (perquisite (event, &ftp))
        return;

    ftp.guild_player->equalizer = "0"; // new_equalizer;
    ftp.guild_player->set_equalizer = true;

    event.reply ("Resetting...");
}

void
slash_run (const dpp::slashcommand_t &event)
{
    std::string arg_action = "";
    get_inter_param (event, "action", &arg_action);

    // automatically fallback to "Set" if any argument other than action
    // provided
    if (arg_action.empty ())
        {
            int64_t temp = 0;
            for (size_t i = 0; i < eq_options_size; i++)
                {
                    // if param isn't provided
                    if (get_inter_param (event, eq_options[i][0], &temp) != 0)
                        continue;

                    // param provided, set action and break
                    arg_action = action_t_e.set;
                    break;
                }
        }

    handle_command ({ arg_action, action_handlers, event });
}

} // musicat::command::filters::equalizer

// 1b=0.5:2b=0.5:3b=0.5:4b=0.5:5b=0.5:6b=0.5:7b=0.5:8b=0.5:9b=0.5:10b=0.5:11b=0.5:12b=0.5:13b=0.5:14b=0.5:15b=0.5:16b=0.5:17b=0.5:18b=0.5
