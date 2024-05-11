#include "musicat/child/worker.h"
#include "musicat/child.h"
#include "musicat/child/command.h"
#include "musicat/child/slave_manager.h"
#include "musicat/child/worker_command.h"
#include "musicat/musicat.h"
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

static int read_fd = -1;
static int write_fd = -1;

int
execute (command::command_options_t &options)
{
    int status = 0;

    auto slave_info = slave_manager::get_slave (options.id);
    std::string ready_msg (command::command_options_keys_t.ready);

    if (slave_info.first == 0 && slave_info.second.command == options.command)
        {
            fprintf (stderr,
                     "[child::worker_command ERROR] Slave with Id and command "
                     "already exist: %s %s\n",
                     options.id.c_str (), options.command.c_str ());

            status = ready_status_t.ERR_SLAVE_EXIST;
            goto ret;
        }

    if (options.command
        == command::command_execute_commands_t.create_audio_processor)
        {
            status = worker_command::create_audio_processor (options);
        }
    else if (options.command == command::command_execute_commands_t.shutdown)
        {
            if (slave_info.first != 0)
                return slave_info.first;

            slave_manager::shutdown (options.id);
            slave_manager::wait (options.id, options.force);
            slave_manager::clean_up (options.id);

            return 0;
        }
    else if (options.command == command::command_execute_commands_t.call_ytdlp)
        {
            status = worker_command::call_ytdlp (options);
        }
    else if (options.command
             == command::command_execute_commands_t.call_system)
        {
            status = worker_command::call_system (options);
        }

    if (status == 0)
        {
            slave_manager::insert_slave (options);
        }

ret:
    ready_msg += '=' + std::to_string (status) + ';'
                 + command::command_options_keys_t.id + '=' + options.id + ';'
                 + command::command_options_keys_t.command + '='
                 + command::command_options_keys_t.ready + ';';

    command::write_command (ready_msg, write_fd, "child::worker::execute");

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
close_fds ()
{
    close_valid_fd (&read_fd);
    close_valid_fd (&write_fd);
}

void
handle_worker_fork ()
{
    close_fds ();
    slave_manager::handle_worker_fork ();
}

// run as main's child process, is helpers/slaves manager
void
run ()
{
    char cmd[CMD_BUFSIZE + 1];

    // main_loop
    size_t read_size = 0;
    while ((read_size = read (read_fd, cmd, CMD_BUFSIZE)) > 0)
        {
            cmd[read_size] = '\0';
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
            perror ("[child::worker::main_loop ERROR] read");
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
            perror ("[child::worker::create_pipe ERROR] pipe");

            return { -1, -1 };
        }

    return { fds[0], fds[1] };
}

pid_t
call_fork (const char *debug_child_name)
{
    // struct sigaction sa = {};

    // sa.sa_handler = SIG_DFL;
    // if (sigaction (SIGCHLD, &sa, nullptr) == -1)
    //     perror ("[child::worker::call_fork] sigaction");

    pid_t pid = fork ();

    if (pid == -1)
        perror ("[child::worker::call_fork ERROR] fork");
    else if (debug_child_name && pid != 0)
        fprintf (stderr, "[child::worker::call_fork] New child: %s (%d)\n",
                 debug_child_name, pid);

    return pid;
}

int
call_waitpid (pid_t cpid)
{
    int wstatus;
    int exitstatus = -1;
    pid_t w;

    fprintf (stderr, "[child::worker::call_waitpid] Reaping child %d\n",
             cpid);

    do
        {
            w = waitpid (cpid, &wstatus, 0);
            if (w == -1)
                {
                    perror ("[child::worker::call_waitpid ERROR] waitpid");
                    return -1;
                }

            if (WIFEXITED (wstatus))
                {
                    exitstatus = WEXITSTATUS (wstatus);
                }
            else if (WIFSIGNALED (wstatus))
                {
                    exitstatus = WTERMSIG (wstatus);
                    fprintf (stderr,
                             "[child::worker::call_waitpid] %d: received "
                             "signal %d\n",
                             cpid, exitstatus);
                }
        }
    while (!WIFEXITED (wstatus) && !WIFSIGNALED (wstatus));

    fprintf (stderr, "[child::worker::call_waitpid] Child %d reaped\n", cpid);

    return exitstatus;
}

} // worker
} // child
} // musicat
