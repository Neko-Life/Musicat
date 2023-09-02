#include "musicat/child/worker.h"
#include <unistd.h>

namespace musicat
{
namespace child
{
namespace worker_management
{

int
shutdown_audio_processor (worker::command_options_t &options)
{
    std::string child_type = options.command;

    if (child_type
        == worker::command_execute_commands_t.create_audio_processor)
        {
            if (options.parent_read_fd > -1)
                {
                    // close parent read fd now since processor
                    // mainly uses fifo and only have one pipe
                    // connected to parent
                    close (options.parent_read_fd);
                    options.parent_read_fd = -1;
                }
        }

    return 0;
}

} // worker_management
} // child
} // musicat
