#ifndef MUSICAT_COMMAND_FILTERS_MODULATION_H
#define MUSICAT_COMMAND_FILTERS_MODULATION_H

#include "musicat/cmds.h"
#include "musicat/cmds/filters.h"
#include <dpp/dpp.h>

namespace musicat::command::filters::modulation
{

struct setup_subcommand_options_t
{
    const char *subcmd_name;
    const char *subcmd_desc;
    const char *fx_verb;
};

void setup_subcommand (dpp::slashcommand &slash,
                       const setup_subcommand_options_t &options);

void show (const dpp::slashcommand_t &event,
           double (*get_f) (const filters_perquisite_t &),
           int (*get_d) (const filters_perquisite_t &));

void set (const dpp::slashcommand_t &event,
          double (*get_f) (const filters_perquisite_t &),
          int (*get_d) (const filters_perquisite_t &),
          void (*set_f) (const filters_perquisite_t &, double f),
          void (*set_d) (const filters_perquisite_t &, int d));

void reset (const dpp::slashcommand_t &event,
            double (*get_f) (const filters_perquisite_t &),
            int (*get_d) (const filters_perquisite_t &),
            void (*set_f) (const filters_perquisite_t &, double f),
            void (*set_d) (const filters_perquisite_t &, int d));

void slash_run (const dpp::slashcommand_t &event,
                const command_handler_t *action_handlers);

}

#endif // MUSICAT_COMMAND_FILTERS_MODULATION_H
