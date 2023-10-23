#ifndef MUSICAT_RUNTIME_CLI_H
#define MUSICAT_RUNTIME_CLI_H

#include <mutex>
#include <string>

namespace musicat::runtime_cli
{
extern std::mutex ns_mutex; // EXTERN_VARIABLE

bool get_attached_state ();

bool set_attached_state (bool s);

void handle_command (const std::string &cmd_str);

/**
 * @brief Load commands to be accessible to command handlers and initialize
 * padding for help cmd
 *
 * Make sure to always lock `ns_mutex` whenever calling this
 */
void load_commands ();

/**
 * @brief Create listener in a new thread to accept runtime command from stdin
 *
 * @return int 0 on success, 1 if listener already attached
 */
int attach_listener ();

} // musicat::runtime_cli

#endif // MUSICAT_RUNTIME_CLI_H
