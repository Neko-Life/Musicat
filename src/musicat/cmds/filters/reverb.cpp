#include "musicat/cmds/filters.h"
#include "musicat/musicat.h"

namespace musicat::command::filters::reverb
{

void setup_subcommand (dpp::slashcommand &slash) {
    dpp::command_option reverbsubcmd (dpp::co_sub_command, "reverb",
                                    "Reverb[ effect]");

    // reverbsubcmd.add_option (
    //     dpp::command_option (
    //         dpp::co_integer, "rate",
    //         "New sampling rate[ to set]. Specify 48000 to reset[ the fx]")
    //         .set_min_value (MIN_VAL)
    //         .set_max_value (MAX_VAL));

    slash.add_option (reverbsubcmd);
}

void slash_run (const dpp::slashcommand_t &event) {
    filters_perquisite_t ftp;

    if (perquisite (event, &ftp))
        return;
}

} // musicat::command::filters::reverb

// ffmpeg -i - -af aecho=1.0:0.7:20:0.5 -f s16le -ac 2 -threads 1 -nostdin -
// cat AKU\ DAH\ LUPA\ -\ ZIA\ \&\ MIKKY--ktlIHSOOmk.opus | ffmpeg -i - -i ../reverbs/CUSTOM\ gen\ plate.WAV -lavfi afir -f s16le -ac 2 -threads 1 -nostdin - | aplay -f dat -
