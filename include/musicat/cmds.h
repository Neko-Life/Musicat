#ifndef MUSICAT_COMMAND_H
#define MUSICAT_COMMAND_H

#include "musicat/player.h"
#include <dpp/dpp.h>
#include <string>

namespace musicat::command
{
struct command_handler_t
{
    const char *name;
    void (*const handler) (const dpp::slashcommand_t &);
};

struct button_command_t
{
    size_t separator_idx;
    std::string command;
    std::string param;
};

struct button_handler_t
{
    const char *name;
    void (*const handler) (const dpp::button_click_t &,
                           const button_command_t &);
};

using command_handlers_map_t = command_handler_t[];
using button_handlers_map_t = button_handler_t[];

struct handle_command_params_t
{
    const std::string &command_name;
    const command_handler_t *const command_handlers_map;
    const dpp::slashcommand_t &event;
};

struct handle_button_params_t
{
    const button_handler_t *const command_handlers_map;
    const dpp::button_click_t &event;
};

enum handle_command_status_e
{
    HANDLE_SLASH_COMMAND_SUCCESS,
    HANDLE_SLASH_COMMAND_NO_HANDLER,
};

handle_command_status_e handle_command (const handle_command_params_t &params);
handle_command_status_e handle_button (const handle_button_params_t &params);

/**
 * @brief Create dpp::command_option with type integer with
 *        `Yes`(1) and `No`(1) choice
 */
dpp::command_option create_yes_no_option (const std::string &name,
                                          const std::string &description,
                                          bool required = false);

std::pair<player::player_manager_ptr_t, int>
cmd_pre_get_player_manager_ready_werr (const dpp::snowflake &guild_id);

player::player_manager_ptr_t
cmd_pre_get_player_manager_ready (const dpp::slashcommand_t &event);

} // musicat::command

#endif // MUSICAT_COMMAND_H
