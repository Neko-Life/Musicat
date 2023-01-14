#ifndef MUSICAT_RUNTIME_CLI_H
#define MUSICAT_RUNTIME_CLI_H

namespace musicat
{
namespace runtime_cli
{
/**
 * @brief Create listener in a new thread to accept runtime command from stdin
 *
 * @return int 0 on success, 1 if listener already attached
 */
int attach_listener ();
}
}

#endif // MUSICAT_RUNTIME_CLI_H
