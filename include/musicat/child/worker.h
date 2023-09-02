#ifndef MUSICAT_CHILD_WORKER_H
#define MUSICAT_CHILD_WORKER_H

#include <string>

namespace musicat
{
namespace child
{
namespace worker
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
} command_options_keys_t;

struct command_options_t
{
    std::string command;

    std::string file_path;
    bool debug;
    std::string id;
};

void set_fds (int r, int w);

void run ();

} // worker
} // child
} // musicat

#endif // MUSICAT_CHILD_WORKER_H
