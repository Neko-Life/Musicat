#include "musicat/child.h"
#include "musicat/child/command.h"
#include "musicat/child/worker.h"
#include "musicat/thread_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <unistd.h>

namespace musicat
{
namespace child
{

int pm_write_fd, pm_read_fd;
pid_t cm_pid;

int
get_parent_write_fd ()
{
    return pm_write_fd;
};

int
init ()
{
    fprintf (stderr, "[child] Initializing...\n");
    thread_manager::print_total_thread ();
    int pipe_fds[4];

    if ((pipe (&pipe_fds[0])) == -1)
        {
            fprintf (stderr, "[child ERROR] Can't create pipe\n");
            perror ("child pipe");
            return ERR_CPIPE;
        }
    pm_write_fd = pipe_fds[1];
    int cm_read_fd = pipe_fds[0];

    if ((pipe (&pipe_fds[2])) == -1)
        {
            fprintf (stderr, "[child ERROR] Can't create pipe\n");
            perror ("child pipe");
            return ERR_CPIPE;
        }
    int cm_write_fd = pipe_fds[3];
    pm_read_fd = pipe_fds[2];

    cm_pid = fork ();
    if (cm_pid < 0)
        {
            fprintf (stderr, "[child ERROR] Can't fork\n");
            perror ("child fork");
            return ERR_CWORKER;
        }

    if (cm_pid == 0)
        {
            close (pm_read_fd);
            close (pm_write_fd);
            pm_read_fd = -1;
            pm_write_fd = -1;

            if (prctl (PR_SET_PDEATHSIG, SIGTERM) == -1)
                {
                    perror ("child prctl");
                    _exit (EXIT_FAILURE);
                }

            worker::set_fds (cm_read_fd, cm_write_fd);
            worker::run ();
        }

    close (cm_read_fd);
    close (cm_write_fd);
    cm_read_fd = -1;
    cm_write_fd = -1;

    // the first thread ever created in the program
    command::run_command_thread ();

    return SUCCESS;
}

void
shutdown ()
{
    fprintf (stderr, "[child] Shutting down...\n");

    command::wake ();

    close (pm_write_fd);

    int cm_status = 0;
    waitpid (cm_pid, &cm_status, 0);
    fprintf (stderr, "[child] Status: %d\n", cm_status);

    close (pm_read_fd);
}

} // child
} // musicat
