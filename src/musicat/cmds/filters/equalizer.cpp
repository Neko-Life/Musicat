/*
Apply 18 band equalizer.

The filter accepts the following options:

1b

    Set 65Hz band gain.
2b

    Set 92Hz band gain.
3b

    Set 131Hz band gain.
4b

    Set 185Hz band gain.
5b

    Set 262Hz band gain.
6b

    Set 370Hz band gain.
7b

    Set 523Hz band gain.
8b

    Set 740Hz band gain.
9b

    Set 1047Hz band gain.
10b

    Set 1480Hz band gain.
11b

    Set 2093Hz band gain.
12b

    Set 2960Hz band gain.
13b

    Set 4186Hz band gain.
14b

    Set 5920Hz band gain.
15b

    Set 8372Hz band gain.
16b

    Set 11840Hz band gain.
17b

    Set 16744Hz band gain.
18b

    Set 20000Hz band gain.

action: set balance reset
up the volume threshold to 500
*/

#include "musicat/cmds.h"
#include "musicat/musicat.h"
#include "musicat/util.h"

namespace musicat
{
namespace command
{
namespace filters
{
namespace equalizer
{
static inline const command_handlers_map_t action_handlers = {
    { "", show_setting },
};

void
show_setting (const dpp::slashcommand_t &event)
{
}

void
setup_subcommand (dpp::slashcommand &slash)
{
}

void
slash_run (const dpp::slashcommand_t &event)
{
    // argument
    std::string arg_action = "";
    get_inter_param (event, "action", &arg_action);

    handle_command ({ arg_action, action_handlers, event });
}

} // equalizer

} // filters
} // command
} // musicat
