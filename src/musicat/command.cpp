#include "musicat/cmds.h"

namespace musicat
{
namespace command
{

handle_command_status_e
handle_command (const handle_command_params_t &params)
{
    auto handler = params.command_handlers_map.find (params.command_name);

    if (handler == params.command_handlers_map.end ())
        {
            fprintf (stderr,
                     "[command::handle_command] No handler registered for "
                     "command: %s\n",
                     params.command_name.c_str ());

            return HANDLE_SLASH_COMMAND_NO_HANDLER;
        }

    handler->second (params.event);

    return HANDLE_SLASH_COMMAND_SUCCESS;
}

} // command
} // musicat
