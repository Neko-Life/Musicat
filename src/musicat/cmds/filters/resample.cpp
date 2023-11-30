#include "musicat/cmds/filters.h"
#include "musicat/musicat.h"

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

    if (rate == 0 || rate == ftp.guild_player->sampling_rate)
        {
            if (ftp.guild_player->sampling_rate == -1)
                {
                    event.reply (
                        "Currently playing at default sampling rate (48KHz)");

                    return;
                }

            event.reply ("Current playback sampling rate is "
                         + std::to_string (ftp.guild_player->sampling_rate)
                         + "Hz");

            return;
        }

    if (rate < MIN_VAL)
        rate = MIN_VAL;

    if (rate > MAX_VAL)
        rate = MAX_VAL;

    bool no_rate = rate == 48000;

    int64_t new_rate = no_rate ? -1 : rate;

    ftp.guild_player->sampling_rate = new_rate;
    ftp.guild_player->set_sampling_rate = true;

    if (new_rate == -1)
        {
            event.reply ("Resetting resampler");
            return;
        }

    event.reply ("Setting rate to " + std::to_string (new_rate) + "Hz");
}

} // musicat::command::filters::resample
