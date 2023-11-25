#include "musicat/cmds/filters/modulation.h"
#include "musicat/cmds.h"
#include "musicat/cmds/filters.h"
#include "musicat/musicat.h"
#include <dpp/dpp.h>
#include <string>

namespace musicat::command::filters::modulation
{

void
setup_subcommand (dpp::slashcommand &slash,
                  const setup_subcommand_options_t &options)
{
    dpp::command_option subcmd (dpp::co_sub_command, options.subcmd_name,
                                options.subcmd_desc);

    std::string str_fx_verb = std::string (options.fx_verb);

    subcmd
        .add_option (
            dpp::command_option (dpp::co_string, "action",
                                 "What you wanna do?", false)
                .add_choice (dpp::command_option_choice ("Set", "0"))
                .add_choice (dpp::command_option_choice ("Reset", "1")))

        .add_option (
            dpp::command_option (
                dpp::co_number, "frequency",
                str_fx_verb + " frequency: [0.1 - 20000.0], default 5.0Hz")
                .set_min_value (0.1)
                .set_max_value (20000.0))
        .add_option (dpp::command_option (
                         dpp::co_integer, "intensity",
                         str_fx_verb + " intensity in percent, default 50%")
                         .set_min_value (0)
                         .set_max_value (100));

    slash.add_option (subcmd);
}

void
show (const dpp::slashcommand_t &event,
      double (*get_f) (const filters_perquisite_t &),
      int (*get_d) (const filters_perquisite_t &))
{
    filters_perquisite_t ftp;

    if (perquisite (event, &ftp))
        return;

    bool f_set = false, d_set = false;
    std::string rep;

    double freq = get_f (ftp);

    if (freq != -1)
        {
            f_set = true;

            rep += "Frequency: " + std::to_string (freq);
        }

    int depth = get_d (ftp);

    if (depth != -1)
        {
            d_set = true;

            if (f_set)
                {
                    rep += '\n';
                }

            rep += "Intensity: " + std::to_string (depth) + "%";
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
set (const dpp::slashcommand_t &event,
     double (*get_f) (const filters_perquisite_t &),
     int (*get_d) (const filters_perquisite_t &),
     void (*set_f) (const filters_perquisite_t &, double f),
     void (*set_d) (const filters_perquisite_t &, int d))
{
    filters_perquisite_t ftp;

    if (perquisite (event, &ftp))
        return;

    double f = -1;
    int64_t d = -1;

    get_inter_param (event, "frequency", &f);
    get_inter_param (event, "intensity", &d);

    std::string rep;

    double ftp_f = get_f (ftp);
    int ftp_d = get_d (ftp);

    bool had_f = ftp_f != -1;
    bool had_d = ftp_d != -1;

    bool should_set_f = f != -1;
    bool should_set_d = d != -1;

    if (!should_set_f && had_f)
        {
            f = ftp_f;
            should_set_f = true;
        }

    if (!should_set_d && had_d)
        {
            d = ftp_d;
            should_set_d = true;
        }

    bool same_f = f == ftp_f;
    bool same_d = d == ftp_d;

    if (!should_set_f && !should_set_d)
        {
            rep += "What do you want to set??";
            goto cmd_reply;
        }

    if (!should_set_f)
        goto skip_set_f;

    if (!same_f)
        {
            if (f < 0.1)
                f = 0.1;

            if (f > 20000.0)
                f = 20000.0;

            same_f = f == ftp_f;

            if (!same_f)
                set_f (ftp, f);
        }

    rep += "Setting frequency to " + std::to_string (get_f (ftp)) + "Hz";

skip_set_f:
    if (!should_set_d)
        goto cmd_reply;

    if (!same_d)
        {
            if (d < 0)
                d = 0;

            if (d > 100)
                d = 100;

            same_d = d == ftp_d;

            if (!same_d)
                {
                    set_d (ftp, d);
                }
        }

    if (should_set_f)
        rep += '\n';

    rep += "Setting intensity to " + std::to_string (get_d (ftp)) + "%";

cmd_reply:
    event.reply (rep);
}

void
reset (const dpp::slashcommand_t &event,
       double (*get_f) (const filters_perquisite_t &),
       int (*get_d) (const filters_perquisite_t &),
       void (*set_f) (const filters_perquisite_t &, double f),
       void (*set_d) (const filters_perquisite_t &, int d))
{
    filters_perquisite_t ftp;

    if (perquisite (event, &ftp))
        return;

    if (get_f (ftp) == -1 && get_d (ftp) == -1)
        {
            event.reply ("This filter is not enabled, please enable first "
                         "before resetting");

            return;
        }

    set_f (ftp, -1);
    set_d (ftp, -1);

    event.reply ("Resetting...");
}

void
slash_run (const dpp::slashcommand_t &event,
           const command_handler_t *action_handlers)
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

} // musicat::command::filters::modulation
