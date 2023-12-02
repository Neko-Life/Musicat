#include "musicat/cmds/filters.h"
#include "musicat/musicat.h"

#define MIN_VAL 0.5
#define MAX_VAL 4.0

namespace musicat::command::filters::tempo
{

void
setup_subcommand (dpp::slashcommand &slash)
{
    dpp::command_option ratesubcmd (dpp::co_sub_command, "tempo",
                                    "Adjust[ playback] tempo");

    ratesubcmd.add_option (
        dpp::command_option (dpp::co_number, "speed",
                             "Allowed range: [0.5-4.0], higher is faster. "
                             "Specify 1 to reset[ to original] tempo")
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

    double rate = 1.0;
    int param_status = get_inter_param (event, "speed", &rate);

    if (param_status == -1 || rate == ftp.guild_player->tempo)
        {
            if (ftp.guild_player->tempo == 1.0)
                {
                    event.reply ("Currently playing at original tempo");
                    return;
                }

            char ctempo[5];

            int ctempo_siz
                = snprintf (ctempo, 5, "%.2lf", ftp.guild_player->tempo);

            event.reply ("Current playback tempo is "
                         + std::string (ctempo, ctempo_siz) + "x");

            return;
        }

    if (rate < MIN_VAL)
        rate = MIN_VAL;

    if (rate > MAX_VAL)
        rate = MAX_VAL;

    ftp.guild_player->tempo = rate;
    ftp.guild_player->set_tempo = true;

    if (rate == 1.0)
        {
            event.reply ("Resetting tempo");
            return;
        }

    char ctempo[5];

    int ctempo_siz = snprintf (ctempo, 5, "%.2lf", ftp.guild_player->tempo);

    event.reply ("Setting tempo to " + std::string (ctempo, ctempo_siz) + "x");
}

} // musicat::command::filters::tempo
