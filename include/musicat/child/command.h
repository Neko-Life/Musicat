#ifndef MUSICAT_CHILD_COMMAND_H
#define MUSICAT_CHILD_COMMAND_H

#include <string>

namespace musicat
{
namespace child
{
namespace command
{

void command_queue_routine ();

void wait_for_command ();

void run_command_thread ();

int send_command (std::string &cmd);

void wake ();

void write_command (std::string &cmd);

// should be called before send_command when setting value
// and use the return string as value
std::string sanitize_command_value (const std::string &value);

// mostly internal use
std::string sanitize_command_key_value (const std::string &key_value);

} // command
} // child
} // musicat

#endif // MUSICAT_CHILD_COMMAND_H
