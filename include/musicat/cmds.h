#ifndef MUSICAT_COMMAND_H
#define MUSICAT_COMMAND_H

#include <dpp/dpp.h>
#include <string>

namespace musicat::command
{
struct command_handler_t
{
    const char *name;
    void (*const handler) (const dpp::slashcommand_t &);
};

using command_handlers_map_t = command_handler_t[];

struct handle_command_params_t
{
    const std::string &command_name;
    const command_handler_t *const command_handlers_map;
    const dpp::slashcommand_t &event;
};

enum handle_command_status_e
{
    HANDLE_SLASH_COMMAND_SUCCESS,
    HANDLE_SLASH_COMMAND_NO_HANDLER,
};

handle_command_status_e handle_command (const handle_command_params_t &params);

/**
 * @brief Create dpp::command_option with type integer with
 *        `Yes`(1) and `No`(1) choice
 */
dpp::command_option create_yes_no_option (const std::string &name,
                                          const std::string &description,
                                          bool required = false);

} // musicat::command

#endif // MUSICAT_COMMAND_H
