#include "musicat/child/system.h"
#include "musicat/child/command.h"
#include <linux/prctl.h>
#include <stdlib.h>
#include <sys/poll.h>
#include <sys/prctl.h>

namespace musicat::child::system
{

std::string
get_system_fifo_path (const std::string &id)
{
    return std::string ("/tmp/musicat.") + id + ".sys";
}

std::string
get_system_json_filename (const std::string &id)
{
    return std::string ("/tmp/musicat.") + id + ".sys.out";
}

bool
revents_has_err (short revents)
{
    return revents & POLLERR || revents & POLLHUP || revents & POLLNVAL;
}

size_t
manage_pollfds_stack (pollfd *pfds, std::vector<char> &fd_tracker)
{
    size_t fdsiz = fd_tracker.size ();
    std::vector<size_t> removed_fds;
    removed_fds.reserve (fdsiz);

    for (size_t i = 0; i < fdsiz; i++)
        {
            if (!revents_has_err (pfds[i].revents))
                continue;

            removed_fds.push_back (i);
        }

    size_t mv_back = 0;
    for (size_t i = 0; i < fdsiz; i++)
        {
            bool is_removed = false;

            auto ri = removed_fds.begin ();
            while (ri != removed_fds.end ())
                {
                    if (*ri != i)
                        {
                            ri++;
                            continue;
                        }

                    close (pfds[i].fd);
                    fd_tracker.erase (fd_tracker.begin ()
                                      + (long)(i - mv_back));
                    is_removed = true;

                    removed_fds.erase (ri);
                    break;
                }

            if (mv_back > 0)
                {
                    pfds[i - mv_back].fd = pfds[i].fd;
                    pfds[i - mv_back].events = pfds[i].events;
                }

            if (is_removed)
                {
                    mv_back++;
                }
        }

    return fd_tracker.size ();
}

size_t
cmp_available (size_t want, size_t avail)
{
    return want > avail ? avail : want;
}

int
process_outfile (const command::command_options_t &options, int read_fd,
                 int readerr_fd)
{
    bool out_stderr = !options.sys_no_stderr;
    bool mark_stderr = options.sys_w_stderr_mark;

    std::string outfname = get_system_json_filename (options.id);
    // open out json file, no need to delete it for cache!
    FILE *outfile = fopen (outfname.c_str (), "w");

    // read and write output to file
    if (!outfile)
        return -1;

    std::vector<char> fd_tracker;
    fd_tracker.reserve (2);

    struct pollfd prfds[2];
    prfds[0].events = POLLIN;
    prfds[0].fd = read_fd;
    fd_tracker.push_back ('o');

    nfds_t prfds_size = 1;

    if (out_stderr)
        {
            prfds[1].events = POLLIN;
            prfds[1].fd = readerr_fd;
            fd_tracker.push_back ('e');

            prfds_size++;
        }
    else
        {
            close (readerr_fd);
            readerr_fd = -1;
        }

    constexpr size_t bsize = 4096;
    char buf[bsize];
    ssize_t read_size = 0;
    size_t out_siz = 0;
    while (prfds_size > 0)
        {
            int has_event = poll (prfds, prfds_size, 50);
            // no event for 50 ms
            if (has_event == 0)
                continue;

            nfds_t new_prfds_size = manage_pollfds_stack (prfds, fd_tracker);
            if (prfds_size != new_prfds_size)
                {
                    prfds_size = new_prfds_size;
                    continue;
                }

            constexpr const char err_mark[] = "==========================="
                                              "============= V STDERR V "
                                              "==========================="
                                              "=============\n";

            constexpr const char err_mark_end[]
                = "\n==========================="
                  "============= ^ STDERR ^ "
                  "==========================="
                  "=============\n";

            constexpr size_t len_mark = sizeof (err_mark) / sizeof (*err_mark);

            constexpr size_t len_mark_end
                = sizeof (err_mark_end) / sizeof (*err_mark_end);

            for (nfds_t i = 0; i < prfds_size; i++)
                {
                    bool read_ready = prfds[i].revents & POLLIN;
                    bool is_out = fd_tracker.at (i) == 'o';
                    bool wrote_stderr = false;

                    size_t max_out_len = options.sys_max_out_len;
                    size_t max_outsiz = 0;
                    while (
                        read_ready
                        && ((read_size = read (prfds[i].fd, buf, bsize)) > 0))
                        {
                            size_t sub = 0;
                            size_t this_iter_outsiz
                                = bsize
                                  + (!is_out ? (len_mark + len_mark_end) : 0);

                            if (max_out_len > 0)
                                {
                                    size_t r = out_siz + this_iter_outsiz;

                                    if (r > max_out_len)
                                        sub = r - max_out_len;
                                }

                            max_outsiz = this_iter_outsiz - sub;

                            if (!is_out && !wrote_stderr && mark_stderr
                                && max_outsiz > 0)
                                {
                                    size_t c_w = fwrite (
                                        err_mark, sizeof (*err_mark),
                                        cmp_available (len_mark, max_outsiz),
                                        outfile);

                                    out_siz += c_w;
                                    max_outsiz -= c_w;

                                    wrote_stderr = true;
                                }

                            if (max_outsiz > 0)
                                {
                                    size_t c_w = fwrite (
                                        buf, 1,
                                        cmp_available (read_size, max_outsiz),
                                        outfile);

                                    out_siz += c_w;
                                    max_outsiz -= c_w;
                                }

                            struct pollfd pfd[1];
                            pfd[0].events = POLLIN;
                            pfd[0].fd = prfds[i].fd;
                            has_event = poll (pfd, 1, 0);

                            if (has_event == -1)
                                break;

                            read_ready = has_event > 0
                                         && (pfd[0].revents & POLLIN)
                                         && !revents_has_err (pfd[0].revents);
                        }

                    if (is_out || !wrote_stderr || !mark_stderr
                        || max_outsiz == 0)
                        continue;

                    out_siz += fwrite (
                        err_mark_end, sizeof (*err_mark_end),
                        cmp_available (len_mark_end, max_outsiz), outfile);
                }
        }

    fclose (outfile);
    outfile = NULL;

    return 0;
}

/**
 * Call system
 *
 * Requires sys_cmd (the command gonna be passed to system()),
 * sys_max_out_len (max output length), sys_no_stderr (hide stderr output),
 * sys_w_stderr_mark (mark stderr output) command is call_system, and ofc
 * id too
 *
 * Write to an output file and write to out fifo with file path
 *
 * Caller manage (eg. manually delete) the output file after this job
 * exited
 */
int
run (const command::command_options_t &options, sem_t *sem,
     const std::string &sem_full_key)
{
    int write_fifo = -1;
    int status = -1;
    int outfile_status = -1;
    int read_fd = -1, write_fd = -1;
    int readerr_fd = -1, writeerr_fd = -1;
    std::pair<int, int> ppfds;

    // result options to write to parent fifo
    std::string resopt;

    std::string outfname = get_system_json_filename (options.id);

    // options shortcut
    bool out_stderr = !options.sys_no_stderr;

    // notification fifo path
    std::string as_fp = get_system_fifo_path (options.id);

    // create pipe to get call output
    ppfds = worker::create_pipe ();

    read_fd = ppfds.first;

    if (read_fd == -1)
        {
            status = 3;
            goto exit_failure;
        }

    write_fd = ppfds.second;

    ppfds = worker::create_pipe ();

    readerr_fd = ppfds.first;

    if (read_fd == -1)
        {
            status = 3;
            goto exit_failure;
        }

    writeerr_fd = ppfds.second;

    // run query
    status = fork ();

    if (status < 0)
        {
            perror ("worker_command::system::run fork_child");
            status = 2;
            goto exit_failure;
        }

    if (status == 0)
        {
            if (prctl (PR_SET_PDEATHSIG, SIGTERM) == -1)
                {
                    perror ("call_system prctl");
                    child::do_sem_post (sem);
                    _exit (EXIT_FAILURE);
                }

            // call system
            close (read_fd);
            read_fd = -1;
            close (readerr_fd);
            readerr_fd = -1;

            // redirect stdout to write_fd
            dup2 (write_fd, STDOUT_FILENO);
            close (write_fd);
            write_fd = -1;

            int errredir = -1;
            if (out_stderr)
                {
                    // redirect stderr to write_fd
                    errredir = writeerr_fd;
                }
            else
                {
                    // else redirect stderr to /dev/null
                    errredir = open ("/dev/null", O_WRONLY);
                }

            dup2 (errredir, STDERR_FILENO);
            close (writeerr_fd);
            writeerr_fd = -1;

            if (!out_stderr)
                close (errredir);

            child::do_sem_post (sem);

            ::system (options.sys_cmd.c_str ());

            _exit (EXIT_SUCCESS);
        }

    close (write_fd);
    write_fd = -1;
    close (writeerr_fd);
    writeerr_fd = -1;

    child::do_sem_wait (sem, sem_full_key);

    write_fifo = open (as_fp.c_str (), O_WRONLY);

    if (write_fifo < 0)
        {
            perror ("worker_command::system::run write_fifo open");
            status = 1;
            goto exit_failure;
        }

    outfile_status = process_outfile (options, read_fd, readerr_fd);

    // write output fp to write_fifo
    resopt += command::create_arg_sanitize_value (
                  command::command_options_keys_t.id, options.id)
              + command::create_arg (
                  command::command_options_keys_t.command,
                  command::command_execute_commands_t.call_system)
              + command::create_arg_sanitize_value (
                  command::command_options_keys_t.file_path,
                  (outfile_status == 0 ? outfname : ""));

    command::write_command (resopt, write_fifo,
                            "worker_command::system::run resopt");

    close (write_fifo);
    write_fifo = -1;

exit_failure:
    return status;
}

} // musicat::child::system
