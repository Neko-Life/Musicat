#include "musicat/audio_processing.h"
#include "musicat/musicat.h"
#include <assert.h>
#include <fcntl.h>
#include <iostream>
#include <poll.h>
#include <stdio.h>
#include <string>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

#define BUFFER_SIZE processing_buffer_size
#define MINIMUM_WRITE 128000

#define OUT_CMD "pipe:1"

namespace musicat
{
namespace audio_processing
{

// p*fd = fd for parent
// c*fd = fd for child
static int
run_ffmpeg (track_data_t *p_track, parent_child_ic_t *p_info)
{
    // request kernel to kill little self when parent dies
    if (prctl (PR_SET_PDEATHSIG, SIGTERM) == -1)
        {
            perror ("child");
            _exit (EXIT_FAILURE);
        }

    int preadfd = p_info->ppipefd[0];
    int cwritefd = p_info->ppipefd[1];
    int creadfd = p_info->cpipefd[0];
    int pwritefd = p_info->cpipefd[1];

    close (pwritefd); /* Close unused write end */
    close (preadfd);  /* Close unused read end */

    // redirect ffmpeg stdin and stdout to prepared pipes
    // exit if unable to redirect
    int status;

    status = dup2 (creadfd, STDIN_FILENO);
    close (creadfd);
    if (status == -1)
        {
            perror ("din");
            _exit (EXIT_FAILURE);
        }

    status = dup2 (cwritefd, STDOUT_FILENO);
    close (cwritefd);
    if (status == -1)
        {
            perror ("dout");
            _exit (EXIT_FAILURE);
        }

    // redirect ffmpeg stderr to /dev/null
    int dnull = open ("/dev/null", O_WRONLY);
    dup2 (dnull, STDERR_FILENO);
    close (dnull);

    std::string block_size_str ("16");

    execlp ("ffmpeg", "ffmpeg", "-f", "s16le", "-i", "pipe:0", "-blocksize",
            block_size_str.c_str (), "-af",
            (std::string ("volume=")
             + std::to_string ((float)p_track->player->volume / (float)100))
                .c_str (),
            "-f", "s16le",
            /*"-preset", "ultrafast",*/ OUT_CMD, (char *)NULL);

    perror ("ffmpeg");
    _exit (EXIT_FAILURE);
}

run_processor_error_t
run_processor (track_data_t *p_track, parent_child_ic_t *p_info)
{
    const bool debug = get_debug_state ();
    // decode opus track
    FILE *input = popen ((std::string ("opusdec ") + p_track->track->filename
                          + " - 2>/dev/null")
                             .c_str (),
                         "r");

    if (!input)
        {
            if (debug)
                fprintf (stderr, "popen\n");
            return ERR_INPUT;
        }

    // prepare required pipes for bidirectional interprocess communication
    if (pipe (p_info->ppipefd) == -1)
        {
            perror ("ppipe");
            return ERR_SPIPE;
        }
    int preadfd = p_info->ppipefd[0];
    int cwritefd = p_info->ppipefd[1];

    if (pipe (p_info->cpipefd) == -1)
        {
            perror ("cpipe");
            return ERR_SPIPE;
        }
    int creadfd = p_info->cpipefd[0];
    int pwritefd = p_info->cpipefd[1];

    // create a child
    p_info->cpid = fork ();
    if (p_info->cpid == -1)
        {
            perror ("fork");
            return ERR_SFORK;
        }

    if (p_info->cpid == 0)
        { /* Child reads from pipe */
            run_ffmpeg (p_track, p_info);
        }

    run_processor_error_t error_status = SUCCESS;

    close (creadfd);  /* Close unused read end */
    close (cwritefd); /* Close unused write end */

    // prepare required data for polling ffmpeg stdout
    struct pollfd pfds[1];
    pfds[0].events = POLLIN;

    pfds[0].fd = preadfd;

    // associate fds with FILEs for better write and read handling
    FILE *pwritefile = fdopen (pwritefd, "w");
    FILE *preadfile = fdopen (preadfd, "r");

    // main loop
    int write_attempt = 0;
    float current_volume = (float)p_track->player->volume;
    size_t read_size = 0;
    size_t minimum_write = 0;
    char buffer[BUFFER_SIZE];

    while ((read_size = fread (buffer, 1, BUFFER_SIZE, input)) > 0)
        {
            if (debug)
                fprintf (stderr, "attempt to write: %d\n", ++write_attempt);
            size_t written_size = 0;
            while (
                (written_size += fwrite (buffer + written_size, 1,
                                         read_size - written_size, pwritefile))
                < read_size)
                ; // keep writing until buffer entirely written

            minimum_write += written_size;
            if (debug)
                fprintf (stderr, "minimum write: %ld\n", minimum_write);

            // poll ffmpeg stdout
            int has_event = poll (pfds, 1, 0);
            const bool read_ready
                = (has_event > 0) && (pfds[0].revents & POLLIN);

            // check if this might be the last data we can read
            if (read_size < BUFFER_SIZE)
                {
                    if (debug)
                        fprintf (stderr, "Last straw of data, closing pipe\n");
                    // close pipe to send eof to child
                    fclose (pwritefile);
                    pwritefile = NULL;
                }

            // if minimum_write was hit, meaning the fd was ready before
            // the call to poll, poll is reporting for event, not reporting
            // if data is waiting to read or not
            if (read_ready || (minimum_write >= MINIMUM_WRITE))
                {
                    while (
                        (read_size = fread (buffer, 1, BUFFER_SIZE, preadfile))
                        > 0)
                        {
                            fwrite (buffer, 1, read_size, stdout);

                            // check if this might be the last data we can read
                            if (read_size < BUFFER_SIZE)
                                break;

                            // poll again to see if there's activity after read
                            has_event = poll (pfds, 1, 0);
                            if ((has_event > 0) && (pfds[0].revents & POLLIN))
                                continue;
                            else
                                break;
                        }

                    // reset state after successful write and read
                    write_attempt = 0;
                    minimum_write = 0;
                }

            // recreate ffmpeg process to update filter chain
            if (p_track->player->volume != (int)current_volume)
                {
                    // close opened files
                    fclose (pwritefile);
                    pwritefile = NULL;
                    fclose (preadfile);
                    preadfile = NULL;

                    // wait for child to finish transferring data
                    int status;
                    waitpid (p_info->cpid, &status, 0);
                    if (debug)
                        fprintf (stderr, "child status: %d\n", status);

                    // kill child
                    kill (p_info->cpid, SIGTERM);

                    // wait for child until it completely died to prevent it
                    // becoming zombie
                    waitpid (p_info->cpid, &status, 0);
                    if (debug)
                        fprintf (stderr, "killed child status: %d\n", status);
                    assert (waitpid (p_info->cpid, &status, 0) == -1);

                    // do the same setup routine as startup
                    if (pipe (p_info->ppipefd) == -1)
                        {
                            perror ("ppipe");
                            error_status = ERR_LPIPE;
                            break;
                        }
                    preadfd = p_info->ppipefd[0];
                    cwritefd = p_info->ppipefd[1];

                    if (pipe (p_info->cpipefd) == -1)
                        {
                            perror ("cpipe");
                            error_status = ERR_LPIPE;
                            break;
                        }
                    creadfd = p_info->cpipefd[0];
                    pwritefd = p_info->cpipefd[1];

                    p_info->cpid = fork ();
                    if (p_info->cpid == -1)
                        {
                            // error clean up
                            close (creadfd);
                            close (pwritefd);
                            close (cwritefd);
                            close (preadfd);
                            perror ("fork");
                            error_status = ERR_LFORK;
                            break;
                        }

                    if (p_info->cpid == 0)
                        { /* Child reads from pipe */
                            run_ffmpeg (p_track, p_info);
                        }

                    close (creadfd);  /* Close unused read end */
                    close (cwritefd); /* Close unused write end */

                    // update fd to poll
                    pfds[0].fd = preadfd;

                    pwritefile = fdopen (pwritefd, "w");
                    preadfile = fdopen (preadfd, "r");

                    // mark changes done
                    current_volume = (float)p_track->player->volume;
                }
        }

    // no more data to read from input, clean up
    pclose (input);
    if (debug)
        fprintf (stderr, "input closed\n");

    if (pwritefile)
        fclose (pwritefile);

    if (preadfile)
        fclose (preadfile);

    // kill child
    kill (p_info->cpid, SIGTERM);

    int status;
    if (debug)
        fprintf (stderr, "waiting for child\n");
    waitpid (p_info->cpid, &status, 0); /* Wait for child */
    if (debug)
        fprintf (stderr, "killed child status: %d\n", status);
    assert (waitpid (p_info->cpid, &status, 0) == -1);

    return error_status;
} // run_processor

} // audio_processing
} // musicat
