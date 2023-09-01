#include "musicat/child/worker.h"
#include "musicat/child.h"
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

worker_command_options_t
create_worker_command_options ()
{
    return { "", "", false };
}

int
set_option (worker_command_options_t &options, std::string &cmd_option)
{
    std::string opt = "";
    std::string value = "";

    bool filling_value = false;
    for (const char &c : cmd_option)
        {
            if (c == '=')
                {
                    filling_value = true;
                    continue;
                }

            if (filling_value)
                {
                    value += c;
                }
            else
                {
                    opt += c;
                }
        }

    if (opt == worker_command_options_keys_t.command)
        {
            options.command = value;
        }
    else if (opt == worker_command_options_keys_t.file_path)
        {
            options.file_path = value;
        }
    else if (opt == worker_command_options_keys_t.debug)
        {
            options.debug = opt == "1";
        }

    return 0;
}

int
execute (worker_command_options_t &options)
{
    if (options.command
        == worker_command_execute_commands_t.create_audio_processor)
        {
            return worker_command::create_audio_processor (options);
        }

    return 0;
}

int
handle_command (std::string cmd)
{
    worker_command_options_t options = create_worker_command_options ();

    std::string temp_str = "";
    for (const char &c : cmd)
        {
            if (c == ';')
                {
                    set_option (options, temp_str);
                    temp_str = "";
                    continue;
                }

            temp_str += c;
        }

    if (temp_str.length ())
        {
            set_option (options, temp_str);
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

    // !TODO: write all in message_queue to parent

    // !TODO: remove this and store all child id and pid in cache
    // and wait one by one
    wait (NULL);

    close (write_fd);

    if (read_size < 0)
        {
            fprintf (stderr, "[child::worker ERROR] Error reading command\n");
            perror ("child::worker main_loop");
            _exit (read_size);
        }

    _exit (SUCCESS);
};

} // worker
} // child
} // musicat
