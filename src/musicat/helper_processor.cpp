#include "musicat/helper_processor.h"
#include "musicat/audio_processing.h"
#include "musicat/child/worker.h"
#include "musicat/musicat.h"
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
            child::do_sem_post (options.sem);
            _exit (EXIT_FAILURE);
        }

    int status;
    status = dup2 (options.child_write_fd, STDOUT_FILENO);
    close_valid_fd (&options.child_write_fd);
    if (status == -1)
        {
            perror ("helper_processor::helper_main dout");
            child::do_sem_post (options.sem);
            _exit (EXIT_FAILURE);
        }

    status = dup2 (options.child_read_fd, STDIN_FILENO);
    close_valid_fd (&options.child_read_fd);
    if (status == -1)
        {
            perror ("helper_processor::helper_main din");
            child::do_sem_post (options.sem);
            _exit (EXIT_FAILURE);
        }

    const bool debug = get_debug_state ();

    // redirect ffmpeg stderr to /dev/null
    if (!debug)
        {
            // redirect ffmpeg stderr to /dev/null
            int dnull = open ("/dev/null", O_WRONLY);
            dup2 (dnull, STDERR_FILENO);
            close (dnull);
        }

    char *args[] = { "ffmpeg",
                     "-v",
                     "debug",
#ifdef FFMPEG_REALTIME
                     "-probesize",
                     "32",
#endif
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

    if (debug)
        for (unsigned long i = 0; i < (sizeof (args) / sizeof (args[0])); i++)
            {
                if (!args[i])
                    break;

                fprintf (stderr, "%s\n", args[i]);
            }

    child::do_sem_post (options.sem);
    execvp ("ffmpeg", args);

    perror ("helper_processor::helper_main exit");
    _exit (EXIT_FAILURE);
}

int
create_helper (const audio_processing::helper_chain_option_t &hco,
               void (*on_fork) (), const std::string &id)
{
    helper_chain_t helper_process;

    std::pair<int, int> fip, sep;
    pid_t pid;

    const std::string fdbg = "helper_main:" + id;

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
        = child::get_sem_key (std::to_string (helper_process.pid) + "_" + id);

    helper_process.sem = child::create_sem (helper_process.sem_full_key);

    pid = child::worker::call_fork (fdbg.c_str ());
    if (pid == -1)
        {
            child::clear_sem (helper_process.sem, helper_process.sem_full_key);

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

    child::do_sem_wait (helper_process.sem, helper_process.sem_full_key);

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
            if ((status = create_helper (i, on_fork, options.id)) != 0)
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
                    remove_pending (&pending_write, hc.write_fd);
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

    if (new_fds_n == 0)
        {
            // last fd to process, free resources and return early
            int fd = pfds[ni].fd;

            char mode = cpy_fds_state.at (ni);
            bool read_mode = mode == 'r';

            remove_pending (read_mode ? &pending_read : &pending_write, fd);

            close (fd);

            free (pfds);
            pfds = NULL;

            fds_n = new_fds_n;

            return NULL;
        }

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

            char mode = cpy_fds_state.at (hi);
            bool read_mode = mode == 'r';

            if (hi == ni)
                {
                    remove_pending (read_mode ? &pending_read : &pending_write,
                                    fd);

                    close (fd);
                    continue;
                }

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
            if (!discard_output)
                ssize_t wr = write (prev_fd, buf, buf_size);

            status = 0;

            remove_pending (&pending_read, ni_fd);
            remove_pending (&pending_write, prev_fd);
        }

    return status;
}

ssize_t
read_first_fd_routine (int ni_fd, bool *first_read_fd_ready,
                       bool discard_output = false)
{
    struct pollfd crpfd[1];
    crpfd[0].fd = ni_fd;
    crpfd[0].events = POLLIN;

    uint8_t buf[BUFFER_SIZE];
    ssize_t read_size;

    ssize_t written = 0;
    ssize_t last_wrote_siz = -1;

    while (*first_read_fd_ready
           && (read_size = read (ni_fd, buf, BUFFER_SIZE)) > 0)
        {
            if (discard_output)
                goto skip_write;

            last_wrote_siz
                = audio_processing::write_stdout (buf, &read_size, true);

            if (last_wrote_siz == -1)
                {
                    written = -1;
                    break;
                }

            written += last_wrote_siz;

        skip_write:
            // make sure there's no more buffer by waiting a bit longer
            int crevent = poll (crpfd, 1, 20);

            *first_read_fd_ready
                = (crevent > 0) && (crpfd[0].revents & POLLIN);
        }

    if (written != -1)
        remove_pending (&pending_read, ni_fd);

    return written;
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

    ssize_t written = 0;
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

                    return 0;
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
                            if (got_hup && !allow_read && !allow_write)
                                {
                                    pfds = close_pfds_ni (pfds, ni, fds_n,
                                                          fds_state);

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
                                total_wrote_to_chain = 0;

                            if (first_fd)
                                {
                                    written = read_first_fd_routine (
                                        ni_fd, &first_read_fd_ready,
                                        shutdown_discard_output);

                                    if (written == -1)
                                        // error, bail out afayc!
                                        break;

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

                                    remove_pending (&pending_write, ni_fd);

                                    break;
                                }

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
                        break;
                }

            if (written == -1
                || ((shutdown
                         ? (shutdown_is_last_hup || iter >= MAX_SHUTDOWN_ITER)
                         : ori_buffer_written)
                    && !current_has_read))
                break;
        }

    if (pfds)
        {
            free (pfds);
            pfds = NULL;
        }

    return written;
}

ssize_t
shutdown_chain (bool discard_output)
{
    ssize_t written = run_through_chain (NULL, NULL, discard_output);

    if (!pending_write.empty ())
        {
            std::cerr
                << "[helper_processor::shutdown_chain WARN] Pending write:";
            for (int c : pending_write)
                {
                    std::cerr << " " << c;
                }
            std::cerr << "\n";
        }

    if (!pending_read.empty ())
        {
            std::cerr
                << "[helper_processor::shutdown_chain WARN] Pending read:";
            for (int c : pending_read)
                {
                    std::cerr << " " << c;
                }
            std::cerr << "\n";
        }

    pending_write.clear ();
    pending_read.clear ();

    const bool debug = get_debug_state ();

    // shutdown all helpers
    auto hci = active_helpers.begin ();
    while (hci != active_helpers.end ())
        {
            int child_status = child::worker::call_waitpid (hci->pid);

            if (debug)
                fprintf (stderr,
                         "[helper_processor::manage_processor] chain "
                         "child_status: "
                         "%d `%s`\n",
                         child_status, hci->options.raw_args.c_str ());

            hci = active_helpers.erase (hci);
        }

    return written;
}

} // helper_processor
} // musicat
