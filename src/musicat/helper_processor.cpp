#include "musicat/helper_processor.h"
#include "musicat/audio_processing.h"
#include "musicat/child/worker.h"
#include "musicat/musicat.h"
#include <limits.h>
#include <stdio.h>
#include <sys/poll.h>
#include <sys/prctl.h>
#include <sys/wait.h>

// this may look too many
// but on slow machine lower value
// might just not give it enough time to
// actually finish reading every last
// drop of piped data causing horrible clips
//
// this is more of a infinite loop guard
// than actually to limit number
// of shutdown iteration
#define MAX_SHUTDOWN_ITER 50000

// #define DEBUG_LOG
// #define DEBUG_LOG_2

namespace musicat
{
namespace helper_processor
{
std::deque<helper_chain_t> active_helpers = {};

static std::vector<int> pending_write;
static std::vector<int> pending_read;

ssize_t total_wrote_to_chain = 0;
inline constexpr ssize_t MAX_TOTAL_WROTE_TO_CHAIN = SSIZE_MAX;

void
remove_pending (std::vector<int> *vec, int fd)
{
    auto pi = vec->begin ();
    while (pi != vec->end ())
        {
            if (*pi != fd)
                {
                    pi++;
                    continue;
                }

            pi = vec->erase (pi);
        }
}

bool
is_pending (std::vector<int> *vec, int fd)
{
    auto pi = vec->begin ();
    while (pi != vec->end ())
        {
            if (*pi != fd)
                {
                    pi++;
                    continue;
                }

#ifdef DEBUG_LOG_2
            fprintf (stderr, "fd %d is pending\n", fd);
#endif

            return true;
        }

    return false;
}

void
add_pending (std::vector<int> *vec, int fd)
{
    if (is_pending (vec, fd))
        return;

    vec->push_back (fd);
}

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
            audio_processing::do_sem_post (options.sem);
            _exit (EXIT_FAILURE);
        }

    int status;
    status = dup2 (options.child_write_fd, STDOUT_FILENO);
    close_valid_fd (&options.child_write_fd);
    if (status == -1)
        {
            perror ("helper_processor::helper_main dout");
            audio_processing::do_sem_post (options.sem);
            _exit (EXIT_FAILURE);
        }

    status = dup2 (options.child_read_fd, STDIN_FILENO);
    close_valid_fd (&options.child_read_fd);
    if (status == -1)
        {
            perror ("helper_processor::helper_main din");
            audio_processing::do_sem_post (options.sem);
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

    audio_processing::do_sem_post (options.sem);
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

    helper_process.sem_full_key
        = audio_processing::get_sem_key (std::to_string (helper_process.pid));

    helper_process.sem
        = audio_processing::create_sem (helper_process.sem_full_key);

    pid = fork ();
    if (pid == -1)
        {
            audio_processing::clear_sem (helper_process.sem,
                                         helper_process.sem_full_key);

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

    audio_processing::do_sem_wait (helper_process.sem,
                                   helper_process.sem_full_key);

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

    close_valid_fd (&hci->read_fd);
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
                = (poll (prfds, 1, 100) > 0) && (prfds[0].revents & POLLIN);
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

struct pollfd *
init_helpers_pollfd (nfds_t fds_n, bool close_first_write_fd,
                     std::vector<char> &fds_state)
{
    fds_state.clear ();
    fds_state.reserve (fds_n);

    size_t alloc_size = sizeof (struct pollfd) * fds_n;

    struct pollfd *pfds = (struct pollfd *)malloc (alloc_size);

    if (!pfds)
        {
            fprintf (stderr,
                     "init_helpers_pollfd: Failed allocating %lu bytes\n",
                     alloc_size);

            return NULL;
        }

    nfds_t fdi = 0;

    for (size_t hi = 0; hi < active_helpers.size (); hi++)
        {
            auto &hc = active_helpers.at (hi);

            if (hi == 0 && close_first_write_fd)
                {
                    close_valid_fd (&hc.write_fd);

                    pfds[fdi].fd = hc.read_fd;
                    pfds[fdi++].events = POLLIN;

                    fds_state.push_back ('r');

                    continue;
                }

            // hc
            // 0,      1,     2
            // [read],[write, read]

            pfds[fdi].fd = hc.write_fd;
            pfds[fdi++].events = POLLOUT;

            fds_state.push_back ('w');

            pfds[fdi].fd = hc.read_fd;
            pfds[fdi++].events = POLLIN;

            fds_state.push_back ('r');
        }

    return pfds;
}

struct pollfd *
close_pfds_ni (struct pollfd *pfds, nfds_t ni, nfds_t &fds_n,
               std::vector<char> &fds_state)
{
    if (!pfds)
        return NULL;

    std::vector<char> cpy_fds_state = fds_state;

    nfds_t new_fds_n = fds_n - 1;

    fds_state.clear ();
    fds_state.reserve (new_fds_n);

    size_t alloc_size = sizeof (struct pollfd) * new_fds_n;

    struct pollfd *new_pfds = (struct pollfd *)malloc (alloc_size);

    if (!new_pfds)
        {
            fprintf (stderr, "close_pfds_ni: Failed allocating %lu bytes\n",
                     alloc_size);

            free (pfds);
            pfds = NULL;

            return NULL;
        }

    nfds_t fdi = 0;

    for (nfds_t hi = 0; hi < fds_n; hi++)
        {
            int fd = pfds[hi].fd;

            if (hi == ni)
                {
                    close (fd);
                    continue;
                }

            char mode = cpy_fds_state.at (hi);
            bool read_mode = mode == 'r';

            new_pfds[fdi].fd = fd;
            new_pfds[fdi++].events = read_mode ? POLLIN : POLLOUT;

            fds_state.push_back (mode);
        }

    free (pfds);
    pfds = NULL;

    fds_n = new_fds_n;

    return new_pfds;
}

int
pass_buf (int ni_fd, int prev_fd, uint8_t *buf, ssize_t buf_size, nfds_t ni,
          nfds_t prev_ni, bool discard_output = false)
{
    int status = -1;

    if ((buf_size = read (ni_fd, buf, PROCESSOR_BUFFER_SIZE)) > 0)
        {
#ifdef DEBUG_LOG
            fprintf (stderr,
                     "READ FROM "
                     "%lu: %lu\n",
                     ni, buf_size);
#endif

            if (!discard_output)
                {
                    ssize_t wr = write (prev_fd, buf, buf_size);

#ifdef DEBUG_LOG
                    fprintf (stderr,
                             "WROTE TO "
                             "%lu: %lu\n",
                             prev_ni, wr);
#endif
                }

            status = 0;

            remove_pending (&pending_read, ni_fd);
            remove_pending (&pending_write, prev_fd);
        }

    return status;
}

int
read_first_fd_routine (int ni_fd, bool *first_read_fd_ready,
                       bool discard_output = false)
{
    struct pollfd crpfd[1];
    crpfd[0].fd = ni_fd;
    crpfd[0].events = POLLIN;

    uint8_t buf[BUFFER_SIZE];
    ssize_t read_size;

    int status = -1;

#if defined(DEBUG_LOG) || defined(DEBUG_LOG_2)
    fprintf (stderr,
             "READING FROM "
             "CHAIN: first_read_fd_ready: %d\n",
             *first_read_fd_ready);
#endif

    while (*first_read_fd_ready
           && (read_size = read (ni_fd, buf, BUFFER_SIZE)) > 0)
        {
#if defined(DEBUG_LOG) || defined(DEBUG_LOG_2)
            fprintf (stderr,
                     "READ FROM "
                     "CHAIN: %lu\n",
                     read_size);
#endif

            if (!discard_output)
                audio_processing::write_stdout (buf, &read_size, true);

            status = 0;

            // break on last buffer
            // don't need this when we
            // use standard buffer size
            //
            // if (read_size < (const ssize_t)BUFFER_SIZE)
            //     break;

            // make sure there's no more buffer by waiting a bit longer
            int crevent = poll (crpfd, 1, 20);

            *first_read_fd_ready
                = (crevent > 0) && (crpfd[0].revents & POLLIN);
        }

    if (status == 0)
        remove_pending (&pending_read, ni_fd);

    return status;
}

ssize_t
run_through_chain (uint8_t *buffer, ssize_t *size,
                   bool shutdown_discard_output)
{
    bool shutdown = buffer == NULL && size == NULL;
    ssize_t dummy_size = 0;

    if (!shutdown)
        {
            if (*size == 0)
                return 0;
        }
    else
        {
            size = &dummy_size;
        }

    // ssize_t SET_BUF_SIZE = *size;
    size_t helper_size = active_helpers.size ();

    if (helper_size == 0)
        return 0;

#ifdef DEBUG_LOG
    fprintf (stderr, "SHOULD PROCESS %lu BYTES\n", *size);
#endif

    // gather fds to poll
    // each process have 2 fd, stdin and stdout
    // fd to process = active_helpers * 2
    nfds_t fds_n = ((helper_size * 2));

    if (shutdown)
        fds_n--;

    std::vector<char> fds_state;
    struct pollfd *pfds = init_helpers_pollfd (fds_n, shutdown, fds_state);

    if (!pfds)
        {
            fprintf (stderr,
                     "run_through_chain: Failed allocating memory for pfds\n");

            return -1;
        }

    int event;

    // save original buffer
    std::string ori_buffer;
    if (!shutdown)
        ori_buffer = std::string ((char *)buffer, *size);

    bool ori_buffer_written = false;

    nfds_t max_idx;

    // if this variable got checked then certainly it's true
    bool first_read_fd_ready = true;
    size_t iter = 0;

    bool shutdown_is_last_hup = false;

    while (pfds)
        {
            iter++;
            event = poll (pfds, fds_n, 50);

            // nothing in 50 ms?
            if (!event && !pending_read.size () && !pending_write.size ())
                {
                    free (pfds);
                    pfds = NULL;

                    return 1;
                }

            bool current_has_read = false;

            // reverse loop
            for (nfds_t ni = (max_idx = fds_n - 1);; ni--)
                {
                    bool read_idx = fds_state.at (ni) == 'r';

                    int ni_fd = pfds[ni].fd;
                    short ni_revents = pfds[ni].revents;

                    bool skip_check = false;

                    bool first_fd = ni == max_idx;
                    bool last_fd = ni == 0;

                    // invalid if first_fd=true
                    nfds_t prev_ni = first_fd ? max_idx : ni + 1;
                    int prev_fd = pfds[prev_ni].fd;
                    short prev_revents = pfds[prev_ni].revents;

#ifdef DEBUG_LOG
                    fprintf (stderr,
                             "read_idx: %d ni_fd: %d ni_revents: %d "
                             "skip_check: %d "
                             "first_fd: %d last_fd: %d "
                             "prev_ni: %lu prev_fd: %d prev_revents: %d\n",
                             read_idx, ni_fd, ni_revents, skip_check, first_fd,
                             last_fd, prev_ni, prev_fd, prev_revents);
#endif

                    // invalid if last_fd=true
                    // nfds_t next_ni = last_fd ? 0 : ni - 1;
                    // int next_fd = pfds[next_ni].fd;
                    // short next_revents = pfds[next_ni].revents;

                    bool allow_read = (ni_revents & POLLIN
                                       || is_pending (&pending_read, ni_fd));

                    bool allow_write = (ni_revents & POLLOUT
                                        || is_pending (&pending_write, ni_fd));

                    bool got_hup = ni_revents & POLLHUP;

                    if (shutdown)
                        {
#ifdef DEBUG_LOG_2
                            fprintf (
                                stderr,
                                "SHUTDOWN HUP CHECK first_fd: %d got_hup: %d "
                                "read_idx: %d prev_ni: %lu allow_read: %d "
                                "allow_write: %d\n",
                                first_fd, got_hup, read_idx, prev_ni,
                                allow_read, allow_write);

                            fprintf (stderr, "fds_state:");
                            for (char c : fds_state)
                                {
                                    fprintf (stderr, " %c", c);
                                }
                            fprintf (stderr, "\n");
#endif

                            if (got_hup && !allow_read && !allow_write)
                                {
                                    pfds = close_pfds_ni (pfds, ni, fds_n,
                                                          fds_state);

                                    if (!pfds)
                                        break;

                                    if (first_fd)
                                        {
                                            // all hup, lets break and reap all
                                            // children
                                            shutdown_is_last_hup = true;
                                        }

                                    // close previous write fd
                                    else if (fds_state.at (ni) == 'w')
                                        {
                                            // any error is ignored here
                                            // we're exiting anyway, who cares
                                            //
                                            // ni is subtracted from the above
                                            // close call
                                            pfds = close_pfds_ni (
                                                pfds, ni, fds_n, fds_state);

                                            if (!pfds)
                                                break;
                                        }

                                    break;
                                }

                            // reset first_read_fd_ready state
                            if (!first_read_fd_ready && allow_read && first_fd)
                                first_read_fd_ready = true;
                        }

                    if (read_idx && allow_read)
                        {
                            skip_check = true;

                            if (!shutdown && ni == 1)
                                {
#ifdef DEBUG_LOG_2
                                    fprintf (stderr,
                                             "TOTAL WROTE TO CHAIN ON READ "
                                             "READY: %lu\n",
                                             total_wrote_to_chain);
#endif

                                    total_wrote_to_chain = 0;
                                }

                            if (first_fd)
                                {
                                    int status = read_first_fd_routine (
                                        ni_fd, &first_read_fd_ready,
                                        shutdown_discard_output);

                                    if (status == 0)
                                        current_has_read = true;

                                    if (last_fd)
                                        break;

                                    continue;
                                }

                            // read and pipe to prev write_fd once

                            uint8_t buf[PROCESSOR_BUFFER_SIZE];
                            ssize_t buf_size = 0;

                            // check if next fd is open for writing
                            if (prev_revents & POLLOUT
                                || is_pending (&pending_write, prev_fd))
                                {
                                    int status = pass_buf (
                                        ni_fd, prev_fd, buf, buf_size, ni,
                                        prev_ni, shutdown_discard_output);

                                    if (status == 0)

                                        current_has_read = true;

                                    if (last_fd)
                                        break;

                                    continue;
                                }

#ifdef DEBUG_LOG
                            fprintf (stderr,
                                     "CURRENT: %lu CANT WRITE TO "
                                     "%lu, add to pending read\n",
                                     ni, prev_ni);
#endif

                            // can't write to prev fd, mark this
                            // read_fd pending
                            add_pending (&pending_read, ni_fd);

                            if (last_fd)
                                break;

                            continue;
                        }

                    if (!read_idx && allow_write)
                        {
                            skip_check = true;

                            if (last_fd && *size)
                                {
                                    if (ori_buffer_written)
                                        break;

                                    ssize_t wr
                                        = write (ni_fd, ori_buffer.data (),
                                                 ori_buffer.length ());

                                    total_wrote_to_chain += wr;

                                    *size = 0;

                                    ori_buffer_written = true;

#ifdef DEBUG_LOG
                                    fprintf (stderr, "WROTE TO CHAIN: %lu\n",
                                             wr);
#endif
#ifdef DEBUG_LOG_2
                                    fprintf (stderr,
                                             "TOTAL WROTE TO CHAIN: %lu\n",
                                             total_wrote_to_chain);
#endif

                                    remove_pending (&pending_write, ni_fd);

                                    break;
                                }

#ifdef DEBUG_LOG
                            fprintf (stderr,
                                     "PENDING WRITE "
                                     "%lu\n",
                                     ni);
#endif

                            // mark pending, let the read handler
                            // do all the work
                            add_pending (&pending_write, ni_fd);

                            if (last_fd)
                                break;

                            continue;
                        }

                    bool has_error = (got_hup || ni_revents & POLLERR
                                      || ni_revents & POLLNVAL);

                    if (!skip_check)
                        {
                            if (has_error)
                                {
                                    // remove this fd from pending cache
                                    std::vector<int> *vec;

                                    if (read_idx)
                                        vec = &pending_read;
                                    else
                                        vec = &pending_write;

                                    if (!vec->size ())
                                        {
                                            if (last_fd)
                                                {
                                                    break;
                                                }

                                            continue;
                                        }

                                    remove_pending (vec, ni_fd);

                                    if (last_fd)
                                        break;

                                    continue;
                                }
                        }

                    if (last_fd)
                        {
                            break;
                        }
                }

#if defined(DEBUG_LOG) || defined(DEBUG_LOG_2)
            fprintf (stderr,
                     "ori_buffer_written: %d current_has_read: %d iter: %lu "
                     "shutdown_is_last_hup: %d\n",
                     ori_buffer_written, current_has_read, iter,
                     shutdown_is_last_hup);
#endif

            if ((shutdown ? (shutdown_is_last_hup || iter >= MAX_SHUTDOWN_ITER)
                          : ori_buffer_written)
                && !current_has_read)
                break;
        }

    if (pfds)
        {
            free (pfds);
            pfds = NULL;
        }

#ifdef DEBUG_LOG
    fprintf (stderr, "LOOP DONE: %lu\n", iter);
#endif

    return 0;
}

int
shutdown_chain (bool discard_output)
{
#if defined(DEBUG_LOG) || defined(DEBUG_LOG_2)
    fprintf (stderr, "SHUTTING DOWN CHAIN\n");
#endif

    int status = run_through_chain (NULL, NULL, discard_output);

    pending_write.clear ();
    pending_read.clear ();

#if defined(DEBUG_LOG) || defined(DEBUG_LOG_2)
    fprintf (stderr, "REAPING PROCESSES\n");
#endif

    // shutdown all helpers
    auto hci = active_helpers.begin ();
    while (hci != active_helpers.end ())
        {
            // close_valid_fd (&hci->write_fd);

            // ssize_t buf_size = 0;
            // uint8_t buf[PROCESSOR_BUFFER_SIZE];
            // while ((buf_size = read (hci->read_fd, buf,
            // PROCESSOR_BUFFER_SIZE))
            //        > 0)
            //     {
            //         if (!discard_output)
            //             // write read buffer to stdout
            //             audio_processing::write_stdout (buf, &buf_size,
            //             true);
            //     }

            int status = 0;
            waitpid (hci->pid, &status, 0);

            if (hci->options.debug)
                fprintf (stderr,
                         "[helper_processor::manage_processor] chain "
                         "status: "
                         "%d `%s`\n",
                         status, hci->options.raw_args.c_str ());

            // close_valid_fd (&hci->read_fd);

            hci = active_helpers.erase (hci);
        }

#if defined(DEBUG_LOG) || defined(DEBUG_LOG_2)
    fprintf (stderr, "DONE SHUTTING DOWN\n");
#endif

    return status;
}

} // helper_processor
} // musicat
