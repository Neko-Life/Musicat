#include "musicat/cmds.h"
#include "musicat/cmds/filters.h"
#include "musicat/musicat.h"
#include <dpp/dpp.h>
#include <string>

namespace musicat::command::filters::vibrato
{

void
setup_subcommand (dpp::slashcommand &slash)
{
    dpp::command_option subcmd (dpp::co_sub_command, "vibrato",
                                "Well, vibrate[ playback audio]");

    subcmd
        .add_option (
            dpp::command_option (dpp::co_string, "action",
                                 "What you wanna do?", false)
                .add_choice (dpp::command_option_choice ("Set", "0"))
                .add_choice (dpp::command_option_choice ("Reset", "1")))

        .add_option (dpp::command_option (
                         dpp::co_number, "frequency",
                         "Vibrate frequency: [0.1 - 20000.0], default 5.0Hz")
                         .set_min_value (0.1)
                         .set_max_value (20000.0))
        .add_option (
            dpp::command_option (dpp::co_integer, "intensity",
                                 "Vibrate intensity in percent, default 50%")
                .set_min_value (0)
                .set_max_value (100));

    slash.add_option (subcmd);
}

void
show (const dpp::slashcommand_t &event)
{
    filters_perquisite_t ftp;

    if (perquisite (event, &ftp))
        return;

    bool f_set = false, d_set = false;
    std::string rep;

    if (ftp.guild_player->vibrato_f != -1)
        {
            f_set = true;

            rep += "Frequency: "
                   + std::to_string (ftp.guild_player->vibrato_f);
        }

    if (ftp.guild_player->vibrato_d != -1)
        {
            d_set = true;

            if (f_set)
                {
                    rep += '\n';
                }

            rep += "Intensity: " + std::to_string (ftp.guild_player->vibrato_d)
                   + "%";
        }

    if (f_set && d_set)
        {
            // dummy if to prevent very significantly major performance
            // degradation
        }
    else if (!f_set && !d_set)
        {
            rep += "This filter is not enabled";
        }
    else if (f_set && !d_set)
        {
            rep += "\nDefault intensity";
        }
    else if (!f_set && d_set)
        {
            rep += "\nDefault frequency";
        }

    event.reply (rep);
}

void
set (const dpp::slashcommand_t &event)
{
    filters_perquisite_t ftp;

    if (perquisite (event, &ftp))
        return;

    double f = -1;
    int64_t d = -1;

    get_inter_param (event, "frequency", &f);
    get_inter_param (event, "intensity", &d);

    std::string rep;

    bool had_f = ftp.guild_player->vibrato_f != -1;
    bool had_d = ftp.guild_player->vibrato_d != -1;

    bool set_f = f != -1;
    bool set_d = d != -1;

    if (!set_f && had_f)
        {
            f = ftp.guild_player->vibrato_f;
            set_f = true;
        }

    if (!set_d && had_d)
        {
            d = ftp.guild_player->vibrato_d;
            set_d = true;
        }

    bool same_f = f == ftp.guild_player->vibrato_f;
    bool same_d = d == ftp.guild_player->vibrato_d;

    if (!set_f && !set_d)
        {
            rep += "What do you want to set??";
        }
    else
        {
            if (set_f)
                {
                    if (!same_f)
                        {
                            if (f < 0.1)
                                f = 0.1;

                            if (f > 20000.0)
                                f = 20000.0;

                            same_f = f == ftp.guild_player->vibrato_f;

                            if (!same_f)
                                {
                                    ftp.guild_player->set_vibrato = true;
                                    ftp.guild_player->vibrato_f = f;
                                }
                        }

                    rep += "Setting frequency to "
                           + std::to_string (ftp.guild_player->vibrato_f)
                           + "Hz";
                }

            if (set_d)
                {
                    if (!same_d)
                        {
                            if (d < 0)
                                d = 0;

                            if (d > 100)
                                d = 100;

                            same_d = d == ftp.guild_player->vibrato_d;

                            if (!same_d)
                                {
                                    ftp.guild_player->set_vibrato = true;
                                    ftp.guild_player->vibrato_d = d;
                                }
                        }

                    if (set_f)
                        rep += '\n';

                    rep += "Setting intensity to "
                           + std::to_string (ftp.guild_player->vibrato_d)
                           + "%";
                }
        }

    event.reply (rep);
}

void
reset (const dpp::slashcommand_t &event)
{
    filters_perquisite_t ftp;

    if (perquisite (event, &ftp))
        return;

    if (ftp.guild_player->vibrato_f == -1 && ftp.guild_player->vibrato_d == -1)
        {
            event.reply ("This filter is not enabled, please enable first "
                         "before resetting");

            return;
        }

    ftp.guild_player->vibrato_f = -1;
    ftp.guild_player->vibrato_d = -1;
    ftp.guild_player->set_vibrato = true;

    event.reply ("Resetting...");
}

inline constexpr const command_handlers_map_t action_handlers
    = { { "", show }, { "0", set }, { "1", reset }, { NULL, NULL } };

void
slash_run (const dpp::slashcommand_t &event)
{
    filters_perquisite_t ftp;

    if (perquisite (event, &ftp))
        return;

    std::string arg_action = "";
    get_inter_param (event, "action", &arg_action);

    if (arg_action.empty ())
        {
            double f = -1;
            int64_t d = -1;

            if (get_inter_param (event, "frequency", &f) == 0
                || get_inter_param (event, "intensity", &d) == 0)
                arg_action = "0";
        }

    handle_command ({ arg_action, action_handlers, event });
}

} // musicat::command::filters::resample
