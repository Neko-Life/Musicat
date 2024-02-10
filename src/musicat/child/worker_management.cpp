#include "musicat/child/command.h"
#include <unistd.h>

namespace musicat
{
namespace child
{
namespace worker_management
{

int
shutdown_audio_processor (command::command_options_t &options)
{
    /*
    if (options.parent_read_fd > -1)
        {
            // close parent read fd now since processor
            // mainly uses fifo and only have one pipe
            // connected to parent
            close (options.parent_read_fd);
            options.parent_read_fd = -1;
        }
    */

    return 0;
}

int
clean_up_audio_processor (command::command_options_t &options)
{
    unlink (options.audio_stream_fifo_path.c_str ());
    unlink (options.audio_stream_stdin_path.c_str ());
    unlink (options.audio_stream_stdout_path.c_str ());

    return 0;
}

} // worker_management
} // child
} // musicat
