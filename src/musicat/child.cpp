#include "musicat/child.h"
#include "musicat/child/command.h"
#include "musicat/child/worker.h"
#include "musicat/thread_manager.h"
#include "musicat/util.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <unistd.h>

namespace musicat::child
{

static int pm_write_fd = -1;
static int pm_read_fd = -1;

static pid_t cm_pid = -1;

int *
get_parent_write_fd ()
{
    return &pm_write_fd;
}

int *
get_parent_read_fd ()
{
    return &pm_read_fd;
}

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

    cm_pid = child::worker::call_fork ();
    if (cm_pid < 0)
        {
            fprintf (stderr, "[child ERROR] Can't fork\n");
            perror ("child fork");
            return ERR_CWORKER;
        }

    if (cm_pid == 0)
        {
            if (prctl (PR_SET_PDEATHSIG, SIGTERM) == -1)
                {
                    perror ("child prctl");
                    _exit (EXIT_FAILURE);
                }

            close (pm_read_fd);
            close (pm_write_fd);
            pm_read_fd = -1;
            pm_write_fd = -1;

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

std::string
get_sem_key (const std::string &key)
{
    return std::string ("musicat.") + key + '.'
           + std::to_string (util::get_current_ts ());
}

sem_t *
create_sem (const std::string &full_key)
{
    return sem_open (full_key.c_str (), O_CREAT, 0600, 0);
}

int
do_sem_post (sem_t *sem)
{
    int status = sem_post (sem);
    if (status)
        {
            perror ("do_sem_post");
        }

    return status;
}

int
clear_sem (sem_t *sem, const std::string &full_key)
{
    sem_close (sem);

    sem_unlink (full_key.c_str ());

    return 0;
}

int
do_sem_wait (sem_t *sem, const std::string &full_key)
{
    int status = sem_wait (sem);
    if (status)
        {
            perror ("do_sem_wait");
        }

    clear_sem (sem, full_key);

    return status;
}

} // musicat::child
