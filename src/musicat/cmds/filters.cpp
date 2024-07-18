#include "musicat/cmds/filters.h"
#include "musicat/cmds.h"
#include "musicat/musicat.h"

// status, save, load, delete filters manage subcommand
namespace musicat::command::filters
{

equalizer_fx_t
create_equalizer_fx_t ()
{
    return { 100,
             { 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
               50, 50 } };
}

std::string
band_to_str (float v)
{
    return std::to_string (v / (float)percent_to_dec_base);
}

std::string
vol_to_str (float v)
{
    return std::to_string (v / (float)percent_to_dec_base);
}

std::string
band_vol_to_str_value (int64_t v)
{
    return std::to_string (v);
}

std::string
equalizer_fx_t_to_af_args (const equalizer_fx_t &eq)
{
    return "1b=" + band_to_str ((float)eq.bands[0])
           + ":2b=" + band_to_str ((float)eq.bands[1])
           + ":3b=" + band_to_str ((float)eq.bands[2])
           + ":4b=" + band_to_str ((float)eq.bands[3])
           + ":5b=" + band_to_str ((float)eq.bands[4])
           + ":6b=" + band_to_str ((float)eq.bands[5])
           + ":7b=" + band_to_str ((float)eq.bands[6])
           + ":8b=" + band_to_str ((float)eq.bands[7])
           + ":9b=" + band_to_str ((float)eq.bands[8])
           + ":10b=" + band_to_str ((float)eq.bands[9])
           + ":11b=" + band_to_str ((float)eq.bands[10])
           + ":12b=" + band_to_str ((float)eq.bands[11])
           + ":13b=" + band_to_str ((float)eq.bands[12])
           + ":14b=" + band_to_str ((float)eq.bands[13])
           + ":15b=" + band_to_str ((float)eq.bands[14])
           + ":16b=" + band_to_str ((float)eq.bands[15])
           + ":17b=" + band_to_str ((float)eq.bands[16])
           + ":18b=" + band_to_str ((float)eq.bands[17])
           + ",volume=" + vol_to_str ((float)eq.volume);
}

/*
band-1: 150 band-2: 150 band-3: 120 band-4: 105 band-5: 80 band-6: 75
band-13: 75 band-14: 75 band-15: 80 band-16: 80 band-17: 80 band-18: 80
*/

std::string
equalizer_fx_t_to_slash_args (const equalizer_fx_t &eq)
{
    return "band-1: " + band_vol_to_str_value ((float)eq.bands[0])
           + " band-2: " + band_vol_to_str_value ((float)eq.bands[1])
           + " band-3: " + band_vol_to_str_value ((float)eq.bands[2])
           + " band-4: " + band_vol_to_str_value ((float)eq.bands[3])
           + " band-5: " + band_vol_to_str_value ((float)eq.bands[4])
           + " band-6: " + band_vol_to_str_value ((float)eq.bands[5])
           + " band-7: " + band_vol_to_str_value ((float)eq.bands[6])
           + " band-8: " + band_vol_to_str_value ((float)eq.bands[7])
           + " band-9: " + band_vol_to_str_value ((float)eq.bands[8])
           + " volume: " + band_vol_to_str_value ((float)eq.volume)
           + " band-10: " + band_vol_to_str_value ((float)eq.bands[9])
           + " band-11: " + band_vol_to_str_value ((float)eq.bands[10])
           + " band-12: " + band_vol_to_str_value ((float)eq.bands[11])
           + " band-13: " + band_vol_to_str_value ((float)eq.bands[12])
           + " band-14: " + band_vol_to_str_value ((float)eq.bands[13])
           + " band-15: " + band_vol_to_str_value ((float)eq.bands[14])
           + " band-16: " + band_vol_to_str_value ((float)eq.bands[15])
           + " band-17: " + band_vol_to_str_value ((float)eq.bands[16])
           + " band-18: " + band_vol_to_str_value ((float)eq.bands[17]);
}

equalizer_fx_t
af_args_to_equalizer_fx_t (const std::string &str)
{
    equalizer_fx_t ret;

    /*
superequalizer=
1b=1.000000:2b=1.000000:3b=1.000000:4b=1.000000:5b=1.000000:6b=1.000000:7b=1.000000:8b=0.010000:9b=0.010000:10b=0.010000:11b=0.010000:12b=0.010000:13b=0.010000:14b=0.010000:15b=0.010000:16b=0.010000:17b=0.010000:18b=1.000000,volume=2.000000
    */

    constexpr const char *searches[]
        = { "volume=", "1b=",  "2b=",  "3b=",  "4b=",  "5b=",  "6b=",
            "7b=",     "8b=",  "9b=",  "10b=", "11b=", "12b=", "13b=",
            "14b=",    "15b=", "16b=", "17b=", "18b=" };

    int idx = 0;
    for (const char *search : searches)
        {
            auto i = str.find (search);
            auto e = str.find (":", i + 1);

            auto t = i + strlen (search);

            float val = std::stof (
                str.substr (t, e == std::string::npos ? e : e - t));

            if (!idx)
                {
                    ret.volume = (int64_t)(val * percent_to_dec_base);
                }
            else
                {
                    ret.bands[idx - 1] = (int64_t)(val * percent_to_dec_base);
                }

            idx++;
        }

    return ret;
}

// ================================================================================

static inline constexpr const command_handlers_map_t subcommand_handlers
    = { { "equalizer", equalizer::slash_run },
        { "equalizer_presets", equalizer_presets::slash_run },
        { "resample", resample::slash_run },
        { "earwax", earwax::slash_run },
        { "vibrato", vibrato::slash_run },
        { "tremolo", tremolo::slash_run },
        { "tempo", tempo::slash_run },
        { "pitch", pitch::slash_run },
        { NULL, NULL } };

dpp::slashcommand
get_register_obj (const dpp::snowflake &sha_id)
{
    dpp::slashcommand slash ("filters", "Playback filters", sha_id);

    // setup command
    equalizer::setup_subcommand (slash);
    equalizer_presets::setup_subcommand (slash);
    resample::setup_subcommand (slash);
    earwax::setup_subcommand (slash);
    vibrato::setup_subcommand (slash);
    tremolo::setup_subcommand (slash);
    tempo::setup_subcommand (slash);
    pitch::setup_subcommand (slash);

    return slash;
}

int
perquisite (const dpp::slashcommand_t &event, filters_perquisite_t *fpt)
{
    // perquisite
    auto player_manager = cmd_pre_get_player_manager_ready (event);
    if (player_manager == NULL)
        return 1;

    fpt->player_manager = player_manager;

    auto player = player_manager->get_player (event.command.guild_id);

    if (!player)
        {
            event.reply ("No active player in this guild");
            return 1;
        }

    fpt->guild_player = player;

    return 0;
}

void
slash_run (const dpp::slashcommand_t &event)
{
    auto inter = event.command.get_command_interaction ();

    if (inter.options.begin () == inter.options.end ())
        {
            fprintf (stderr, "[command::filters::slash_run ERROR] !!! No "
                             "options for command 'filters' !!!\n");

            event.reply ("I don't have that command, how'd you get that?? "
                         "Please report this to my developer");

            return;
        }

    const std::string cmd = inter.options.at (0).name;

    // run subcommand
    handle_command ({ cmd, subcommand_handlers, event });
}

} // musicat::command::filters
