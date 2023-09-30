#include "musicat/helper_processor.h"
#include "musicat/audio_processing.h"
#include "musicat/child/worker.h"
#include "musicat/musicat.h"
#include <sys/poll.h>
#include <sys/prctl.h>
#include <sys/wait.h>

namespace musicat
{
namespace helper_processor
{
std::deque<helper_chain_t> active_helpers = {};

void
close_all_helper_fds ()
{
    for (helper_chain_t &i : active_helpers)
        {
            close_valid_fd (&i.child_write_fd);
            close_valid_fd (&i.child_read_fd);
            close_valid_fd (&i.write_fd);
            close_valid_fd (&i.read_fd);
        }
}

void
helper_main (helper_chain_t &options)
{
    // request kernel to kill little self when parent dies
    if (prctl (PR_SET_PDEATHSIG, SIGTERM) == -1)
        {
            perror ("helper_processor::helper_main prctl");
            _exit (EXIT_FAILURE);
        }

    int status;
    status = dup2 (options.child_write_fd, STDOUT_FILENO);
    close_valid_fd (&options.child_write_fd);
    if (status == -1)
        {
            perror ("helper_processor::helper_main dout");
            _exit (EXIT_FAILURE);
        }

    status = dup2 (options.child_read_fd, STDIN_FILENO);
    close_valid_fd (&options.child_read_fd);
    if (status == -1)
        {
            perror ("helper_processor::helper_main din");
            _exit (EXIT_FAILURE);
        }

    // redirect ffmpeg stderr to /dev/null
    if (!options.options.debug)
        {
            // redirect ffmpeg stderr to /dev/null
            int dnull = open ("/dev/null", O_WRONLY);
            dup2 (dnull, STDERR_FILENO);
            close (dnull);
        }

    char *args[] = { "ffmpeg",
                     "-v",
                     "debug",
                     "-f",
                     "s16le",
                     "-ac",
                     "2",
                     "-ar",
                     "48000",
                     "-i",
                     "pipe:0",
                     "-af",
                     (char *)options.options.raw_args.c_str (),
                     "-f",
                     "s16le",
                     "-ac",
                     "2",
                     "-ar",
                     "48000",
                     /*"-preset", "ultrafast",*/ "-threads",
                     "1",
                     "-nostdin",
                     "pipe:1",
                     (char *)NULL };

    if (options.options.debug)
        for (unsigned long i = 0; i < (sizeof (args) / sizeof (args[0])); i++)
            {
                if (!args[i])
                    break;

                fprintf (stderr, "%s\n", args[i]);
            }

    execvp ("ffmpeg", args);

    perror ("helper_processor::helper_main exit");
    _exit (EXIT_FAILURE);
}

int
create_helper (const audio_processing::helper_chain_option_t &hco,
               void (*on_fork) ())
{
    helper_chain_t helper_process;

    std::pair<int, int> fip, sep;
    pid_t pid;

    fip = child::worker::create_pipe ();
    if (fip.first == -1)
        goto err1;

    sep = child::worker::create_pipe ();
    if (sep.first == -1)
        goto err2;

    helper_process.options = hco;
    helper_process.read_fd = fip.first;
    helper_process.child_write_fd = fip.second;
    helper_process.child_read_fd = sep.first;
    helper_process.write_fd = sep.second;

    pid = fork ();
    if (pid == -1)
        {
            goto err3;
        }

    helper_process.pid = pid;

    if (pid == 0)
        {
            // close parent fds
            close (helper_process.write_fd);
            close (helper_process.read_fd);
            helper_process.write_fd = -1;
            helper_process.read_fd = -1;

            // close every unrelated fds
            on_fork ();

            helper_main (helper_process);
        }

    // close child fds
    close (helper_process.child_write_fd);
    close (helper_process.child_read_fd);
    helper_process.child_write_fd = -1;
    helper_process.child_read_fd = -1;

    active_helpers.push_back (helper_process);

    return 0;

err3:
    close (sep.first);
    close (sep.second);
err2:
    close (fip.first);
    close (fip.second);
err1:
    return -1;
}

void
handle_first_chain_stop (std::deque<helper_chain_t>::iterator hci,
                         bool is_last_p, bool discard_output)
{
    close_valid_fd (&hci->write_fd);

    ssize_t buf_size = 0;
    uint8_t buf[PROCESSOR_BUFFER_SIZE];
    while ((buf_size = read (hci->read_fd, buf, PROCESSOR_BUFFER_SIZE)) > 0)
        {
            // is also last
            if (is_last_p)
                {
                    if (!discard_output)
                        // write read buffer to stdout
                        audio_processing::write_stdout (buf, &buf_size, true);

                    continue;
                }

            // is first but not also last
            // pipe it to the next processor
            const helper_chain_t &nhc = *(hci + 1);

            write (nhc.write_fd, buf, buf_size);
        }

    // reap zombie chain process
    int status = 0;
    waitpid (hci->pid, &status, 0);

    if (hci->options.debug)
        fprintf (stderr,
                 "[helper_processor::manage_processor] chain "
                 "status: "
                 "%d `%s`\n",
                 status, hci->options.raw_args.c_str ());
}

// shutting down and streaming have the same exact routine
void
handle_middle_chain (std::deque<helper_chain_t>::iterator hci)
{
    // poll and pipe it to the next processor
    const helper_chain_t &nhc = *(hci + 1);

    struct pollfd prfds[1];
    prfds[0].events = POLLIN;
    prfds[0].fd = hci->read_fd;

    bool read_ready = (poll (prfds, 1, 0) > 0) && (prfds[0].revents & POLLIN);

    ssize_t buf_size = 0;
    uint8_t buf[PROCESSOR_BUFFER_SIZE];
    while (
        read_ready
        && ((buf_size = read (hci->read_fd, buf, PROCESSOR_BUFFER_SIZE)) > 0))
        {
            write (nhc.write_fd, buf, buf_size);

            read_ready
                = (poll (prfds, 1, 0) > 0) && (prfds[0].revents & POLLIN);
        }
}

void
handle_last_chain_stop (std::deque<helper_chain_t>::iterator hci)
{
    // drain polled output while writing to stdout
    struct pollfd prfds[1];
    prfds[0].events = POLLIN;
    prfds[0].fd = hci->read_fd;

    bool read_ready
        = (poll (prfds, 1, 100) > 0) && (prfds[0].revents & POLLIN);

    ssize_t buf_size = 0;
    uint8_t buf[PROCESSOR_BUFFER_SIZE];
    while (
        read_ready
        && ((buf_size = read (hci->read_fd, buf, PROCESSOR_BUFFER_SIZE)) > 0))
        {
            audio_processing::write_stdout (buf, &buf_size, true);

            read_ready
                = (poll (prfds, 1, 0) > 100) && (prfds[0].revents & POLLIN);
        }
}

void
stop_first_chain (bool discard_output)
{
    // loop through current active chain to deplete the first chain's buffer
    // then we can proceed to remove it after
    size_t max_idx = active_helpers.size () - 1;
    for (size_t i = max_idx; i >= 0; i--)
        {
            bool is_last_p = i == max_idx;
            bool is_first_p = i == 0;

            auto hci = active_helpers.begin () + i;

            // is first
            //
            // close write fd and based on case:
            // 1. is also last processor:
            //      - drain output to stdout without polling
            // 2. is not last:
            //      - drain output while piping it to the next
            //        processor
            if (is_first_p)
                {
                    handle_first_chain_stop (hci, is_last_p, discard_output);

                    // make sure to break at the end of loop
                    // as this is the index 0 (final index)
                    break;
                }

            // neither last nor first
            else if (!is_last_p)
                {
                    // pipe polled output to next processor
                    handle_middle_chain (hci);

                    continue;
                }

            // is last
            // drain polled output while writing to stdout
            handle_last_chain_stop (hci);
        }
}

/*

Cases that can happen:
1. No required helper and no active helper
2. There's required helper and no active helper, need to create required helper
3. No required helper but there's active helper, need to shutdown active helper
4. There's required helper and active ones, in which can be 2 case:
    1. Both required and active helpers are the exact same args and index,
       do nothing
    2. Changed required helpers, need to reorder/shutdown/create helper to
       match required helper, pretty tricky so it's simply shutting everything
       down and creating it all over for now

*/

int
manage_processor (const audio_processing::processor_options_t &options,
                  void (*on_fork) ())
{
    size_t required_chain_size = options.helper_chain.size (),
           current_chain_size = active_helpers.size ();

    // no required helper and no active helper
    if (!required_chain_size && !current_chain_size)
        return 0;

    bool need_stop = false;

    if (required_chain_size == current_chain_size)
        {
            for (size_t i = 0; i < required_chain_size; i++)
                {
                    const audio_processing::helper_chain_option_t &hco
                        = options.helper_chain[i];
                    const audio_processing::helper_chain_option_t &chco
                        = active_helpers[i].options;

                    // check if there's order/args difference
                    // !TODO: check for debug too? is it worth restarting just
                    // for debug mode?
                    if (hco.raw_args != chco.raw_args)
                        {
                            need_stop = true;
                            break;
                        }
                }
        }
    // length difference, should stop/restart all chain
    else
        need_stop = true;

    if (!need_stop)
        {
            // nothing needs to be done
            return 0;
        }

    // stop all active chain, hci is automatically end if no entry in deque
    shutdown_chain ();

    bool need_start = required_chain_size > 0;

    if (!need_start)
        {
            // nothing needs to be done
            return 0;
        }

    // start all required chain
    int status = 0;
    for (const audio_processing::helper_chain_option_t &i :
         options.helper_chain)
        {
            if ((status = create_helper (i, on_fork)) != 0)
                {
                    fprintf (stderr,
                             "[helper_processor::manage_processor ERROR] "
                             "Failed creating effect chain: %d `%s`\n",
                             status, i.raw_args.c_str ());

                    return status;
                }
        }

    // guaranteed zero status (success) if it got here
    return status;
}

ssize_t
run_through_chain (uint8_t *buffer, ssize_t *size)
{
    if (*size == 0)
        return 0;

    // ssize_t SET_BUF_SIZE = *size;

    size_t current_chain_size = active_helpers.size ();

    if (current_chain_size == 0)
        return 0;

    size_t max_idx = current_chain_size - 1;

    auto hcb = active_helpers.begin ();
    for (size_t i = 0; i <= max_idx; i++)
        {
            bool is_last_p = i == max_idx;
            bool is_first_p = i == 0;

            auto hci = hcb + i;

            if (is_first_p)
                {
                    // handle_first_chain_stream (hci);

                    // pass buffer to processor without polling
                    // whatever happens, this should never fail
                    // not the best way but should work for now

                    /*
struct pollfd prfds[1];
prfds[0].events = POLLOUT;
prfds[0].fd = hci->write_fd;

bool write_ready = (poll (prfds, 1, 1) > 0)
                   && (prfds[0].revents & POLLOUT);

ssize_t wrote = 0, current_w = 0;
while (write_ready && wrote < *size
       && ((current_w // 	s16le stereo minimal frame size
            = write (hci->write_fd, buffer + wrote, 4))
           > 0))
    {
        wrote += current_w;

        write_ready = (poll (prfds, 1, 1) > 0)
                      && (prfds[0].revents & POLLOUT);
    }
                    */

                    // sleep for 0.2 ms
                    usleep (200);

                    write (hci->write_fd, buffer, *size);

                    *size = 0;

                    // if there's deadlock we know where to look
                }

            // not the last chain
            if (!is_last_p)
                {
                    // pipe polled output to next processor
                    handle_middle_chain (hci);

                    continue;
                }

            // is last
            // read polled output to fill out buffer once
            // handle_last_chain_stream (hci);

            struct pollfd prfds[1];
            prfds[0].events = POLLIN;
            prfds[0].fd = hci->read_fd;

            if ((poll (prfds, 1, 0) > 0) && (prfds[0].revents & POLLIN))
                *size = read (hci->read_fd, buffer, BUFFER_SIZE);
        }

    return 0;
}

int
shutdown_chain (bool discard_output)
{
    // stop all active chain, hci is automatically end if no entry in deque
    auto hci = active_helpers.begin ();
    while (hci != active_helpers.end ())
        {
            // error or not, every active helper should die
            stop_first_chain (discard_output);

            // erase the exited chain
            hci = active_helpers.erase (hci);
        }

    return 0;
}

} // helper_processor
} // musicat
