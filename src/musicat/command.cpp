#include "musicat/cmds.h"

namespace musicat::command
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

dpp::command_option
create_yes_no_option (const std::string &name, const std::string &description,
                      bool required)
{
    return dpp::command_option (dpp::co_integer, name, description, required)
        .add_choice (dpp::command_option_choice ("Yes", 1))
        .add_choice (dpp::command_option_choice ("No", 0));
}

} // musicat::command
