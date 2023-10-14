#ifndef MUSICAT_COMMAND_FILTERS_H
#define MUSICAT_COMMAND_FILTERS_H

#include <dpp/dpp.h>
#include "musicat/player.h"

namespace musicat::command::filters
{

struct filters_perquisite_t
{
    player::player_manager_ptr player_manager;
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
} // musicat::command::filters

#endif // MUSICAT_COMMAND_FILTERS_H
