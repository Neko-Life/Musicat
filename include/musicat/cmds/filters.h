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

} // musicat::command::filters

#endif // MUSICAT_COMMAND_FILTERS_H
