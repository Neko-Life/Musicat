#include "musicat/child/system.h"

namespace musicat::child::ytdlp
{

std::string
get_system_fifo_path (const std::string &id)
{
    return std::string ("/tmp/musicat.") + id + ".sys";
}

/*
int
run (const command::command_options_t &options, sem_t *sem,
     const std::string &sem_full_key)
{
    int write_fifo = -1;
    int status = -1;
    int read_fd = -1, write_fd = -1;
    std::pair<int, int> ppfds;

    // result options to write to parent fifo
    std::string resopt;

    // notification fifo path
    std::string as_fp = get_ytdout_fifo_path (options.id);

    // check if outname already exists
    struct stat filestat;
    if (stat (outfname.c_str (), &filestat) == 0
        && (filestat.st_mode & S_IFREG))
        {
            goto write_res;
        }

    // create pipe to get call output
    ppfds = worker::create_pipe ();

    read_fd = ppfds.first;

    if (read_fd == -1)
        {
            status = 3;
            goto exit_failure;
        }

    write_fd = ppfds.second;

    // run query
    status = fork ();

    if (status < 0)
        {
            perror ("worker_command::call_ytdlp fork_child");
            status = 2;
            goto exit_failure;
        }

    if (status == 0)
        {
            // call ytdlp
            close (read_fd);
            read_fd = -1;

            // redirect stdout to write_fd
            dup2 (write_fd, STDOUT_FILENO);
            close (write_fd);
            write_fd = -1;

            // redirect stderr to /dev/null?

            const char *exe = options.ytdlp_util_exe.c_str ();
            const char *query = options.ytdlp_query.c_str ();

            bool has_lib_path = !options.ytdlp_lib_path.empty ();
            bool has_max_entries = options.ytdlp_max_entries > 0;

            const char *lib_path
                = has_lib_path ? options.ytdlp_lib_path.c_str () : "";
            std::string me_str
                = has_max_entries ? std::to_string (options.ytdlp_max_entries)
                                  : "";
            const char *max_entries = me_str.c_str ();

            char *args[32] = {
                "python3",
                (char *)exe,
                (char *)query,
            };
            int args_idx = 3;

            if (has_lib_path)
                {
                    args[args_idx++] = "--ytdlp-dir";
                    args[args_idx++] = (char *)lib_path;
                }

            if (has_max_entries)
                {
                    args[args_idx++] = "--max-entries";
                    args[args_idx++] = (char *)max_entries;
                }

            args[args_idx++] = (char *)NULL;

            audio_processing::do_sem_post (sem);

            execvp ("python3", args);
            _exit (EXIT_FAILURE);
        }

    close (write_fd);
    write_fd = -1;

    audio_processing::do_sem_wait (sem, sem_full_key);

write_res:
    close (read_fd);
    read_fd = -1;

    write_fifo = open (as_fp.c_str (), O_WRONLY);

    if (write_fifo < 0)
        {
            perror ("worker_command::call_ytdlp write_fifo open");
            status = 1;
            goto exit_failure;
        }

    // write output fp to write_fifo
    resopt
        += command::command_options_keys_t.id + '='
           + command::sanitize_command_value (options.id) + ';'
           + command::command_options_keys_t.command + '='
           + command::command_execute_commands_t.call_ytdlp + ';'
           + command::command_options_keys_t.file_path + '='
           + (jsonout_status == 0 ? command::sanitize_command_value (outfname)
                                  : "")
           + ';';

    command::write_command (resopt, write_fifo,
                            "worker_command::call_ytdlp resopt");

    close (write_fifo);
    write_fifo = -1;

exit_failure:
    return status;
}
*/
} // musicat::child::ytdlp
