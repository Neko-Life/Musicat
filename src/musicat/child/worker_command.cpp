#include "musicat/audio_processing.h"
#include "musicat/child.h"
#include "musicat/child/command.h"
#include "musicat/child/slave_manager.h"
#include "musicat/child/worker.h"
#include "musicat/child/ytdlp.h"
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

namespace musicat::child::worker_command
{

int
create_audio_processor (command::command_options_t &options)
{
    // std::pair<int, int> pipe_fds = worker::create_pipe ();
    pid_t status = -1;

    // if (pipe_fds.first == -1)
    //     {
    //         return status;
    //     }
    // int read_fd = pipe_fds.first;
    // int write_fd = pipe_fds.second;

    // !TODO: create fifos here
    std::string as_fp
        = audio_processing::get_audio_stream_fifo_path (options.id);

    std::string si_fp
        = audio_processing::get_audio_stream_stdin_path (options.id);

    std::string so_fp
        = audio_processing::get_audio_stream_stdout_path (options.id);

    std::string sem_full_key;
    sem_t *sem;

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

    sem_full_key = audio_processing::get_sem_key (options.id);
    sem = audio_processing::create_sem (sem_full_key);

    status = fork ();

    if (status < 0)
        {
            perror ("cap fork");
            goto err4;
        }

    if (status == 0)
        {
            worker::handle_worker_fork ();

            // close (read_fd);

            // options.child_write_fd = write_fd;

            audio_processing::do_sem_post (sem);

            status = audio_processing::run_processor (options);
            _exit (status);
        }

    // close (write_fd);

    audio_processing::do_sem_wait (sem, sem_full_key);

    options.pid = status;
    // options.parent_read_fd = read_fd;

    return 0;

err4:
    unlink (so_fp.c_str ());
    audio_processing::do_sem_post (sem);
    audio_processing::do_sem_wait (sem, sem_full_key);
err3:
    unlink (si_fp.c_str ());
err2:
    unlink (as_fp.c_str ());
err1:
    // close (read_fd);
    // close (write_fd);
    return status;
}

/**
 * Invoking ytdlp cmd and save the output to a json file
 *
 * Requires id, ytdlp_util_exe, ytdlp_lib_path and ytdlp_query
 * Optionally ytdlp_max_entries
 *
 * Creates a fifo based on id
 * Caller should open and read the fifo to get the cmd output
 * containing file_path of the result to open and read
 *
 * If file_path is empty then smt wrong happened
 * !TODO: result status?
 */
int
call_ytdlp (command::command_options_t &options)
{
    pid_t status = -1;

    std::string as_fp = ytdlp::get_ytdout_fifo_path (options.id);

    std::string sem_full_key;
    sem_t *sem;

    unlink (as_fp.c_str ());

    const auto fifo_bitmask
        = audio_processing::get_audio_stream_fifo_mode_t ();

    if ((status = mkfifo (as_fp.c_str (), fifo_bitmask)) < 0)
        {
            perror ("worker_command::call_ytdlp as_fp");
            goto err1;
        }

    sem_full_key = audio_processing::get_sem_key (options.id);
    sem = audio_processing::create_sem (sem_full_key);

    status = fork ();

    if (status < 0)
        {
            perror ("worker_command::call_ytdlp fork");
            goto err4;
        }

    if (status == 0)
        {
            worker::handle_worker_fork ();

            audio_processing::do_sem_post (sem);

            int write_fifo = -1;
            status = -1;
            FILE *jsonout = NULL;
            int jsonout_status = -1;
            int read_fd = -1, write_fd = -1;
            std::pair<int, int> ppfds;

            // result options to write to parent fifo
            std::string resopt;

            std::string outfname
                = ytdlp::get_ytdout_json_out_filename (options.id);

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
                        = has_max_entries
                              ? std::to_string (options.ytdlp_max_entries)
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

            // open out json file, no need to delete it for cache!
            jsonout = fopen (outfname.c_str (), "w");

            // read and write output to file
            if (jsonout)
                {
                    constexpr size_t bsize = 4096;
                    char buf[bsize];
                    ssize_t read_size = 0;
                    while ((read_size = read (read_fd, buf, bsize)) > 0)
                        {
                            fwrite (buf, 1, read_size, jsonout);
                        }
                    fclose (jsonout);
                    jsonout = NULL;

                    jsonout_status = 0;
                }

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
            resopt += command::command_options_keys_t.id + '=' + options.id
                      + ';' + command::command_options_keys_t.command + '='
                      + command::command_execute_commands_t.call_ytdlp + ';'
                      + command::command_options_keys_t.file_path + '='
                      + command::sanitize_command_value (outfname) + ';';

            command::write_command (resopt, write_fifo,
                                    "worker_command::call_ytdlp resopt");

            close (write_fifo);
            write_fifo = -1;

        exit_failure:
            _exit (status);
        }

    audio_processing::do_sem_wait (sem, sem_full_key);

    options.pid = status;

    return 0;

err4:
    audio_processing::do_sem_post (sem);
    audio_processing::do_sem_wait (sem, sem_full_key);
    unlink (as_fp.c_str ());
err1:
    return status;
}

} // musicat::child::worker_command
