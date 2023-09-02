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

command_options_t
create_command_options ()
{
    return { "", "", false, "", -1, -1, -1, -1, -1 };
}

int
set_option (command_options_t &options, std::string &cmd_option)
{
    std::string opt = "";
    std::string value = "";

    bool filling_value = false;
    bool include_next_special = false;
    for (const char &c : cmd_option)
        {
            if (!include_next_special)
                {
                    if (c == '\\')
                        {
                            include_next_special = true;
                            continue;
                        }

                    if (c == '=')
                        {
                            filling_value = true;
                            continue;
                        }
                }
            else
                include_next_special = false;

            if (filling_value)
                {
                    value += c;
                    continue;
                }

            opt += c;
        }

    if (opt == command_options_keys_t.id)
        {
            options.id = value;
        }
    else if (opt == command_options_keys_t.command)
        {
            options.command = value;
        }
    else if (opt == command_options_keys_t.file_path)
        {
            options.file_path = value;
        }
    else if (opt == command_options_keys_t.debug)
        {
            options.debug = opt == "1";
        }

    return 0;
}

int
execute (command_options_t &options)
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

    if (options.command == command_execute_commands_t.create_audio_processor)
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
    command_options_t options = create_command_options ();

    bool include_next_special = false;
    std::string temp_str = "";
    for (const char &c : cmd)
        {
            if (!include_next_special)
                {
                    if (c == '\\')
                        {
                            include_next_special = true;
                            continue;
                        }

                    if (c == ';')
                        {
                            std::string opt_str
                                = command::sanitize_command_key_value (
                                    temp_str);
                            set_option (options, opt_str);
                            temp_str = "";
                            continue;
                        }
                }
            else
                include_next_special = false;

            temp_str += c;
        }

    if (temp_str.length ())
        {
            std::string opt_str
                = command::sanitize_command_key_value (temp_str);
            set_option (options, opt_str);
        }

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
