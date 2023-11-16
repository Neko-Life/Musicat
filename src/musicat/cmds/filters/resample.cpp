#include "musicat/cmds/filters.h"
#include "musicat/musicat.h"
#include <dpp/dpp.h>

#define MIN_VAL 6000
#define MAX_VAL 128000

namespace musicat::command::filters::resample
{

void
setup_subcommand (dpp::slashcommand &slash)
{
    dpp::command_option ratesubcmd (dpp::co_sub_command, "resample",
                                    "Modify both [playback ]pitch and tempo");

    ratesubcmd.add_option (
        dpp::command_option (
            dpp::co_integer, "rate",
            "New sampling rate[ to set]. Specify 48000 to reset[ the fx]")
            .set_min_value (MIN_VAL)
            .set_max_value (MAX_VAL));

    slash.add_option (ratesubcmd);
}

void
slash_run (const dpp::slashcommand_t &event)
{
    filters_perquisite_t ftp;

    if (perquisite (event, &ftp))
        return;

    int64_t rate = 0;
    get_inter_param (event, "rate", &rate);

    if (rate == 0)
        {
            if (ftp.guild_player->resample.empty ())
                {
                    event.reply (
                        "Currently playing at original sampling rate");
                    return;
                }

            std::string rate = ftp.guild_player->resample.substr (
                sizeof ("aresample=") - 1);

            event.reply ("Current sampling rate: " + rate);
            return;
        }

    if (rate < MIN_VAL)
        rate = MIN_VAL;

    if (rate > MAX_VAL)
        rate = MAX_VAL;

    bool no_rate = rate == 48000;

    std::string rate_str = no_rate ? "" : std::to_string (rate);

    std::string new_rate = no_rate ? "0" : "aresample=" + rate_str;

    ftp.guild_player->set_resample = new_rate;

    if (rate_str.empty ())
        {
            event.reply ("Resetting resampler");
            return;
        }

    event.reply ("Setting rate to " + rate_str);
}

} // musicat::command::filters::resample
