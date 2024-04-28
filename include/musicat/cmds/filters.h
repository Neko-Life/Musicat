#ifndef MUSICAT_COMMAND_FILTERS_H
#define MUSICAT_COMMAND_FILTERS_H

#include "musicat/player.h"
#include <dpp/dpp.h>

namespace musicat::command::filters
{

struct filters_perquisite_t
{
    player::player_manager_ptr_t player_manager;
    std::shared_ptr<player::Player> guild_player;
};

// ================================================================================

struct equalizer_fx_t
{
    int64_t volume;
    int64_t bands[18];
};

constexpr int64_t percent_to_dec_base = 100;

equalizer_fx_t create_equalizer_fx_t ();

std::string band_to_str (float v);

std::string vol_to_str (float v);

std::string band_vol_to_str_value (int64_t v);

std::string equalizer_fx_t_to_af_args (const equalizer_fx_t &eq);

std::string equalizer_fx_t_to_slash_args (const equalizer_fx_t &eq);

equalizer_fx_t af_args_to_equalizer_fx_t (const std::string &str);

// ================================================================================

dpp::slashcommand get_register_obj (const dpp::snowflake &sha_id);

int perquisite (const dpp::slashcommand_t &event, filters_perquisite_t *fpt);

void slash_run (const dpp::slashcommand_t &event);

namespace equalizer
{
void show (const dpp::slashcommand_t &event);
void set (const dpp::slashcommand_t &event);
void balance (const dpp::slashcommand_t &event);
void reset (const dpp::slashcommand_t &event);

void setup_subcommand (dpp::slashcommand &slash);

void slash_run (const dpp::slashcommand_t &event);

} // equalizer

namespace resample
{

void setup_subcommand (dpp::slashcommand &slash);

void slash_run (const dpp::slashcommand_t &event);

} // resample

namespace earwax
{

void setup_subcommand (dpp::slashcommand &slash);

void slash_run (const dpp::slashcommand_t &event);

} // earwax

namespace vibrato
{

void setup_subcommand (dpp::slashcommand &slash);

void slash_run (const dpp::slashcommand_t &event);

} // vibrato

namespace tremolo
{

void setup_subcommand (dpp::slashcommand &slash);

void slash_run (const dpp::slashcommand_t &event);

} // tremolo

namespace tempo
{

void setup_subcommand (dpp::slashcommand &slash);

void slash_run (const dpp::slashcommand_t &event);

} // tempo

namespace pitch
{

void setup_subcommand (dpp::slashcommand &slash);

void slash_run (const dpp::slashcommand_t &event);

} // pitch

namespace equalizer_presets
{
namespace autocomplete
{
void name (const dpp::autocomplete_t &event, const std::string &param);
} // autocomplete

void setup_subcommand (dpp::slashcommand &slash);

void slash_run (const dpp::slashcommand_t &event);

} // equalizer_presets

namespace list_filters
{

void setup_subcommand (dpp::slashcommand &slash);

void slash_run (const dpp::slashcommand_t &event);

} // list_filters

} // musicat::command::filters

#endif // MUSICAT_COMMAND_FILTERS_H
