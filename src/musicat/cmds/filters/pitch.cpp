#include "musicat/cmds/filters.h"
#include "musicat/musicat.h"

#define MIN_VAL -400
#define MAX_VAL 100

namespace musicat::command::filters::pitch
{

void
setup_subcommand (dpp::slashcommand &slash)
{
    dpp::command_option ratesubcmd (dpp::co_sub_command, "pitch",
                                    "Adjust[ playback] pitch");

    ratesubcmd.add_option (
        dpp::command_option (dpp::co_integer, "percentage",
                             "Allowed range: [-400-100], lower is deeper. "
                             "Specify 0 to reset[ to original] pitch")
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
    int param_status = get_inter_param (event, "percentage", &rate);

    if (param_status == -1 || rate == ftp.guild_player->pitch)
        {
            if (ftp.guild_player->pitch == 0)
                {
                    event.reply ("Currently playing at original pitch");
                    return;
                }

            event.reply ("Current playback pitch is "
                         + std::to_string (ftp.guild_player->pitch) + "%");

            return;
        }

    if (rate < MIN_VAL)
        rate = MIN_VAL;

    if (rate > MAX_VAL)
        rate = MAX_VAL;

    ftp.guild_player->pitch = rate;
    ftp.guild_player->set_pitch = true;

    if (rate == 0)
        {
            event.reply ("Resetting pitch");
            return;
        }

    event.reply ("Setting pitch to " + std::to_string (rate) + "%");
}

} // musicat::command::filters::pitch
