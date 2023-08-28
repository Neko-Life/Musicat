#include "musicat/thread_manager.h"
#include <stdio.h>
// #include <sys/wait.h>

namespace musicat
{
namespace child
{
// int pm_write_fd, pm_read_fd, cm_write_fd, cm_read_fd;
// pid_t cm_pid;

int
init ()
{
    fprintf (stderr, "[CHILD] Initializing...\n");
    thread_manager::print_total_thread ();

    return 0;
}

void
shutdown ()
{
    // close (pm_write_fd);
    // close (pm_read_fd);

    // int cm_status;
    // waitpid (cm_pid, &cm_status, 0);
    fprintf (stderr, "[CHILD] Shutting down...\n");
}

} // child
} // musicat
