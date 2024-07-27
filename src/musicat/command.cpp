#include "musicat/cmds.h"
#include "musicat/musicat.h"
#include "musicat/player.h"

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

int
get_button_cmd (const dpp::button_click_t &event, button_command_t &bcmd)
{
    const size_t fsub = event.custom_id.find ("/");
    const std::string cmd = event.custom_id.substr (0, fsub);

    bcmd.separator_idx = fsub;
    bcmd.command = cmd;

    if (fsub != std::string::npos)
        {
            const std::string param
                = event.custom_id.substr (fsub + 1, std::string::npos);

            bcmd.param = param;
        }

    if (cmd.empty ())
        return 1;

    return 0;
}

handle_command_status_e
handle_button (const handle_button_params_t &params)
{
    button_command_t cmd;

    if (get_button_cmd (params.event, cmd) != 0)
        return HANDLE_SLASH_COMMAND_NO_HANDLER;

    void (*handler) (const dpp::button_click_t &, const button_command_t &)
        = nullptr;

    for (const button_handler_t *i = params.command_handlers_map;
         i->name != NULL; i++)
        {
            if (cmd.command != i->name)
                continue;

            handler = i->handler;
            break;
        }

    if (!handler)
        return HANDLE_SLASH_COMMAND_NO_HANDLER;

    if (cmd.param.empty ())
        fprintf (stderr, "[WARN] button command \"%s\" have no param\n",
                 cmd.command.c_str ());

    handler (params.event, cmd);

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

player::player_manager_ptr_t
cmd_pre_get_player_manager_ready (const dpp::slashcommand_t &event)
{
    auto player_manager = get_player_manager_ptr ();
    if (!player_manager)
        return NULL;

    if (!player_manager->voice_ready (event.command.guild_id))
        {
            event.reply ("Please wait while I'm getting ready to stream");
            return NULL;
        }

    return player_manager;
}

} // musicat::command
