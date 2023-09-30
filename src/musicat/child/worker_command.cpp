#include "musicat/audio_processing.h"
#include "musicat/child.h"
#include "musicat/child/slave_manager.h"
#include "musicat/child/worker.h"
#include <sys/stat.h>
#include <unistd.h>

namespace musicat
{
namespace child
{
namespace worker_command
{

int
create_audio_processor (command::command_options_t &options)
{
    std::pair<int, int> pipe_fds = worker::create_pipe ();
    pid_t status = -1;

    if (pipe_fds.first == -1)
        {
            return status;
        }
    int read_fd = pipe_fds.first;
    int write_fd = pipe_fds.second;

    // !TODO: create fifos here
    std::string as_fp
        = audio_processing::get_audio_stream_fifo_path (options.id);

    std::string si_fp
        = audio_processing::get_audio_stream_stdin_path (options.id);

    std::string so_fp
        = audio_processing::get_audio_stream_stdout_path (options.id);

    unlink (as_fp.c_str ());
    unlink (si_fp.c_str ());
    unlink (so_fp.c_str ());

    const auto fifo_bitmask
        = audio_processing::get_audio_stream_fifo_mode_t ();

    if ((status = mkfifo (as_fp.c_str (), fifo_bitmask)) < 0)
        {
            perror ("cap as_fp");
            goto err1;
        }

    if ((status = mkfifo (si_fp.c_str (), fifo_bitmask)) < 0)
        {
            perror ("cap si_fp");
            goto err2;
        }

    if ((status = mkfifo (so_fp.c_str (), fifo_bitmask)) < 0)
        {
            perror ("cap so_fp");
            goto err3;
        }

    options.audio_stream_fifo_path = as_fp;
    options.audio_stream_stdin_path = si_fp;
    options.audio_stream_stdout_path = so_fp;

    status = fork ();

    if (status < 0)
        {
            perror ("cap fork");
            goto err4;
        }

    if (status == 0)
        {
            worker::handle_worker_fork ();

            close (read_fd);

            options.child_write_fd = write_fd;

            status = audio_processing::run_processor (options);
            _exit (status);
        }

    close (write_fd);

    options.pid = status;
    options.parent_read_fd = read_fd;

    return 0;

err4:
    unlink (so_fp.c_str ());
err3:
    unlink (si_fp.c_str ());
err2:
    unlink (as_fp.c_str ());
err1:
    close (read_fd);
    close (write_fd);
    return status;
}

} // worker_command
} // child
} // musicat
