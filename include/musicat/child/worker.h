#ifndef MUSICAT_CHILD_WORKER_H
#define MUSICAT_CHILD_WORKER_H

#include <sys/types.h>
#include <utility>

namespace musicat::child::worker
{

inline constexpr const struct
{
    const int TIMEOUT = -1;
    const int SUCCESS = 0;
    const int ERR_SLAVE_EXIST = 875;
} ready_status_t;

// yes, afayc is as fast as you can!

inline bool
should_bail_out_afayc (int command_status)
{
    switch (command_status)
        {
        case ready_status_t.ERR_SLAVE_EXIST:
        case ready_status_t.TIMEOUT:
            return true;
        default:
            return false;
        }
}

void set_fds (int r, int w);

void close_fds ();

void handle_worker_fork ();

void run ();

/**
 * @brief Create a one way pipe, read_fd and write_fd
 */
std::pair<int, int> create_pipe ();

pid_t call_fork ();

int call_waitpid (pid_t cpid);

} // musicat::child::worker

#endif // MUSICAT_CHILD_WORKER_H
