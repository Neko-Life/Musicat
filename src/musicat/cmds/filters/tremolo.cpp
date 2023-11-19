#include "musicat/cmds.h"
#include "musicat/cmds/filters.h"
#include "musicat/cmds/filters/modulation.h"
#include "musicat/musicat.h"
#include <dpp/dpp.h>
#include <string>

namespace musicat::command::filters::tremolo
{

static double
get_f (const filters_perquisite_t &ftp)
{
    return ftp.guild_player->tremolo_f;
}

static int
get_d (const filters_perquisite_t &ftp)
{
    return ftp.guild_player->tremolo_d;
}

static void
set_f (const filters_perquisite_t &ftp, double v)
{
    ftp.guild_player->tremolo_f = v;
    ftp.guild_player->set_tremolo = true;
}

static void
set_d (const filters_perquisite_t &ftp, int v)
{
    ftp.guild_player->tremolo_d = v;
    ftp.guild_player->set_tremolo = true;
}

void
setup_subcommand (dpp::slashcommand &slash)
{
    return modulation::setup_subcommand (
        slash, { "tremolo", "Volume cycler", "Cycle" });
}

void
show (const dpp::slashcommand_t &event)
{
    modulation::show (event, get_f, get_d);
}

void
set (const dpp::slashcommand_t &event)
{
    modulation::set (event, get_f, get_d, set_f, set_d);
}

void
reset (const dpp::slashcommand_t &event)
{
    modulation::reset (event, get_f, get_d, set_f, set_d);
}

inline constexpr const command_handlers_map_t action_handlers
    = { { "", show }, { "0", set }, { "1", reset }, { NULL, NULL } };

void
slash_run (const dpp::slashcommand_t &event)
{
    modulation::slash_run (event, action_handlers);
}

} // musicat::command::filters::tremolo
