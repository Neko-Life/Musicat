#include "musicat/child/worker.h"
#include "musicat/child.h"
#include "musicat/child/command.h"
#include "musicat/child/slave_manager.h"
#include "musicat/child/worker_command.h"
#include <deque>
#include <stddef.h>
#include <stdio.h>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

namespace musicat
{
namespace child
{
namespace worker
{
// !TODO: message_queue

int read_fd, write_fd;

int
execute (command::command_options_t &options)
{
    int status = 0;

    auto slave_info = slave_manager::get_slave (options.id);

    if (slave_info.first == 0)
        {
            fprintf (stderr,
                     "[child::worker_command ERROR] Slave with Id already "
                     "exist: %s\n",
                     options.id.c_str ());

            return 1;
        };

    if (options.command == command::command_execute_commands_t.create_audio_processor)
        {
            status = worker_command::create_audio_processor (options);
        }

    if (status == 0)
        slave_manager::insert_slave (options);

    return status;
}

int
handle_command (std::string cmd)
{
    command::command_options_t options = command::create_command_options ();
    parse_command_to_options (cmd, options);

    execute (options);

    return 0;
}

void
set_fds (int r, int w)
{
    read_fd = r;
    write_fd = w;
}

void
run ()
{
    char cmd[CMD_BUFSIZE + 1];

    // main_loop
    size_t read_size = 0;
    while ((read_size = read (read_fd, cmd, CMD_BUFSIZE)) > 0)
        {
            cmd[CMD_BUFSIZE] = '\n';
            fprintf (stderr, "[child::worker] Received command: `%s`\n", cmd);

            handle_command (cmd);
        }

    close (read_fd);

    slave_manager::shutdown_all ();
    slave_manager::wait_all ();
    slave_manager::clean_up_all ();

    // !TODO: write all in message_queue to parent

    close (write_fd);

    if (read_size < 0)
        {
            fprintf (stderr, "[child::worker ERROR] Error reading command\n");
            perror ("child::worker main_loop");
            _exit (read_size);
        }

    _exit (SUCCESS);
};

std::pair<int, int>
create_pipe ()
{
    int fds[2];
    if (pipe (fds) == -1)
        {
            perror ("create_pipe");

            return { -1, -1 };
        }

    return { fds[0], fds[1] };
}

} // worker
} // child
} // musicat