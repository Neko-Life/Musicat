#include "musicat/cmds.h"

namespace musicat
{
namespace command
{

handle_command_status_e
handle_command (const handle_command_params_t &params)
{
    void (*handler) (const dpp::slashcommand_t &) = nullptr;

    for (const command_handler_t *i = params.command_handlers_map;
         i->name != NULL; i++)
        {
            if (params.command_name != i->name)
                continue;

            handler = i->handler;
            break;
        }

    if (!handler)
        return HANDLE_SLASH_COMMAND_NO_HANDLER;

    handler (params.event);

    return HANDLE_SLASH_COMMAND_SUCCESS;
}

} // command
} // musicat
