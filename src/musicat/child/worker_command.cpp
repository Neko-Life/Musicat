#include "musicat/audio_processing.h"
#include "musicat/child/slave_manager.h"
#include "musicat/child/worker.h"
#include <unistd.h>

namespace musicat
{
namespace child
{
namespace worker_command
{

int
create_audio_processor (worker::command_options_t &options)
{
    std::pair<int, int> pipe_fds = worker::create_pipe ();

    if (pipe_fds.first == -1)
        {
            return -1;
        }
    int read_fd = pipe_fds.first;
    int write_fd = pipe_fds.second;

    // !TODO: create fifos here

    pid_t status = fork ();

    if (status < 0)
        {
            close (read_fd);
            close (write_fd);
            return status;
        }

    if (status == 0)
        {
            close (read_fd);

            options.child_write_fd = write_fd;

            status = audio_processing::run_processor (options);
            _exit (status);
        }

    close (write_fd);

    options.pid = status;
    options.parent_read_fd = read_fd;

    return 0;
}

} // worker_command
} // child
} // musicat
