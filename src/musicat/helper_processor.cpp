#include "musicat/helper_processor.h"
#include "musicat/child/worker.h"
#include "musicat/musicat.h"
#include <sys/prctl.h>

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
                     "-",
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
                     "-",
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
stop_chain (helper_chain_t &options)
{
    // !TODO: close child stdin, read until its stdout closes and wait its pid
    close_valid_fd (&options.write_fd);
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
    return 0;
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
    auto hci = active_helpers.begin ();
    while (hci != active_helpers.end ())
        {
            // error or not, every active helper should die
            /* stop_chain (*hci); */

            // !TODO: have a while loop that check the first chain is dead
            size_t max_idx = (current_chain_size = active_helpers.size ()) - 1;
            for (size_t ri = max_idx; ri >= 0; ri--)
                {
                    bool is_last_p = ri == max_idx;
                    bool is_first_p = ri == 0;

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
                            // is not also last
                            if (!is_last_p)
                                {
                                    // pipe it to the next processor
                                }
                            // is also last
                            else
                                {
                                    // write read buffer to stdout
                                }

                            // make sure to break at the end of loop
                            // as this is the index 0 (final index)
                            break;
                        }

                    // neither last nor first
                    else if (!is_last_p)
                        {
                            // pipe polled output to next processor

                            continue;
                        }

                    // is last
                    // drain polled output while writing to stdout
                }

            hci = active_helpers.erase (hci);
        }

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
    /*
    if (*size == 0)
        return 0;
*/
    // !TODO

    return 0;
};

} // helper_processor
} // musicat
