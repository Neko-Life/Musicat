#ifndef MUSICAT_CHILD_COMMAND_H
#define MUSICAT_CHILD_COMMAND_H

#include "musicat/child.h"
#include <string>

namespace musicat
{
namespace child
{
namespace command
{

static const struct
{
    std::string create_audio_processor = "cap";
} command_execute_commands_t;

static const struct
{
    std::string command = "cmd";  // str
    std::string file_path = "fp"; // str
    std::string debug = "dbg";    // bool
    std::string id = "id";        // str
    std::string guild_id = "gid"; // str
    std::string ready = "rdy";    // bool
} command_options_keys_t;

struct command_options_t
{
    std::string command;

    std::string file_path;
    bool debug;
    std::string id;

    pid_t pid;
    int parent_read_fd;
    int parent_write_fd;
    int child_read_fd;
    int child_write_fd;

    std::string audio_stream_fifo_path;
    std::string guild_id;
    bool ready;
};

command_options_t create_command_options ();

void command_queue_routine ();

void wait_for_command ();

void run_command_thread ();

int send_command (std::string &cmd);

void wake ();

// default values are used in the main thread ONLY! You should specify
// write_fd and caller in child processes
void write_command (std::string &cmd, int write_fd = get_parent_write_fd (),
                    const char *caller = "child::command");

// should be called before send_command when setting value
// and use the return string as value
std::string sanitize_command_value (const std::string &value);

// mostly internal use
std::string sanitize_command_key_value (const std::string &key_value);

void parse_command_to_options (std::string &cmd, command_options_t &options);

} // command
} // child
} // musicat

#endif // MUSICAT_CHILD_COMMAND_H
