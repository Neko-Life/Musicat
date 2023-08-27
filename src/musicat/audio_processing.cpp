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

#define SEND_BUFFER_SIZE 11520
#define BUFFER_SIZE processing_buffer_size

#define OUT_CMD "pipe:1"

namespace musicat
{
namespace audio_processing
{

static int
run_reader (std::string &file_path, parent_child_ic_t *p_info)
{
    // request kernel to kill little self when parent dies
    if (prctl (PR_SET_PDEATHSIG, SIGTERM) == -1)
        {
            perror ("child reader");
            _exit (EXIT_FAILURE);
        }
    int prreadfd = p_info->rpipefd[0];
    int crwritefd = p_info->rpipefd[1];

    close (prreadfd); /* Close unused read end */

    int status;
    status = dup2 (crwritefd, STDOUT_FILENO);
    close (crwritefd);
    if (status == -1)
        {
            perror ("reader dout");
            _exit (EXIT_FAILURE);
        }

    // redirect ffmpeg stderr to /dev/null
    int dnull = open ("/dev/null", O_WRONLY);
    dup2 (dnull, STDERR_FILENO);
    close (dnull);

    execlp ("ffmpeg", "ffmpeg", /*"-v", "debug",*/ "-i", file_path.c_str (),
            "-f", "s16le", "-ac", "2", "-ar", "48000",
            /*"-preset", "ultrafast",*/ OUT_CMD, (char *)NULL);

    perror ("reader");
    _exit (EXIT_FAILURE);
}

// p*fd = fd for parent
// c*fd = fd for child
static int
run_ffmpeg (track_data_t *p_track, parent_child_ic_t *p_info)
{
    // request kernel to kill little self when parent dies
    if (prctl (PR_SET_PDEATHSIG, SIGTERM) == -1)
        {
            perror ("child ffmpeg");
            _exit (EXIT_FAILURE);
        }

    const bool debug = get_debug_state ();

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
            perror ("ffmpeg din");
            _exit (EXIT_FAILURE);
        }

    status = dup2 (cwritefd, STDOUT_FILENO);
    close (cwritefd);
    if (status == -1)
        {
            perror ("ffmpeg dout");
            _exit (EXIT_FAILURE);
        }

    if (!debug)
        {
            // redirect ffmpeg stderr to /dev/null
            int dnull = open ("/dev/null", O_WRONLY);
            dup2 (dnull, STDERR_FILENO);
            close (dnull);
        }

    execlp ("ffmpeg", "ffmpeg", "-v", "debug", "-f", "s16le", "-ac", "2",
            "-ar", "48000", "-i", "pipe:0", "-af",
            (std::string ("volume=")
             + std::to_string ((float)p_track->player->volume / (float)100))
                .c_str (),
            "-f", "s16le", "-ac", "2", "-ar", "48000",
            /*"-preset", "ultrafast",*/ OUT_CMD, (char *)NULL);

    perror ("ffmpeg");
    _exit (EXIT_FAILURE);
}

run_processor_error_t
run_processor (track_data_t *p_track, parent_child_ic_t *p_info)
{
    const bool debug = get_debug_state ();

    dpp::discord_voice_client *vclient = p_track->vclient;

    run_processor_error_t init_error = SUCCESS;
    run_processor_error_t error_status = SUCCESS;

    // decode opus track
    if (pipe (p_info->rpipefd) == -1)
        {
            perror ("rpipe");
            return ERR_INPUT;
        }
    int prreadfd = p_info->rpipefd[0];
    int crwritefd = p_info->rpipefd[1];

    p_info->rpid = fork ();
    if (p_info->rpid == -1)
        {
            perror ("rfork");

            close (prreadfd);
            close (crwritefd);

            return ERR_INPUT;
        }

    if (p_info->rpid == 0)
        { /* Child reads from pipe */
            run_reader (p_track->file_path, p_info);
        }

    close (crwritefd);
    crwritefd = -1;

    // declare everything here to be able to use goto
    // fds and poll
    int preadfd, cwritefd, creadfd, pwritefd, cstatus;
    struct pollfd prfds[1], pwfds[1];

    // main loop variable definition
    float current_volume = (float)p_track->player->volume;
    size_t read_size = 0;
    uint8_t buffer[BUFFER_SIZE];
    bool read_input = true;
    size_t written_size = 0;
    uint8_t send_buffer[SEND_BUFFER_SIZE];
    size_t send_buffer_length = 0;

    FILE *input = fdopen (prreadfd, "r");

    // prepare required pipes for bidirectional interprocess communication
    if (pipe (p_info->ppipefd) == -1)
        {
            perror ("ppipe");
            init_error = ERR_SPIPE;
            goto err_spipe1;
        }
    preadfd = p_info->ppipefd[0];
    cwritefd = p_info->ppipefd[1];

    if (pipe (p_info->cpipefd) == -1)
        {
            perror ("cpipe");
            init_error = ERR_SPIPE;
            goto err_spipe2;
        }
    creadfd = p_info->cpipefd[0];
    pwritefd = p_info->cpipefd[1];

    // create a child
    p_info->cpid = fork ();
    if (p_info->cpid == -1)
        {
            perror ("fork");
            init_error = ERR_SFORK;
            goto err_sfork;
        }

    if (p_info->cpid == 0)
        { /* Child reads from pipe */
            run_ffmpeg (p_track, p_info);
        }

    close (creadfd);  /* Close unused read end */
    close (cwritefd); /* Close unused write end */

    creadfd = -1;
    cwritefd = -1;

    // prepare required data for polling
    prfds[0].events = POLLIN;
    pwfds[0].events = POLLOUT;

    prfds[0].fd = preadfd;
    pwfds[0].fd = pwritefd;

    // main loop
    while (true)
        {
            // wait for old pending audio buffer to be sent to discord
            while (vclient && !vclient->terminating
                   && vclient->get_secs_remaining () > 0.05f)
                {
                    std::this_thread::sleep_for (
                        std::chrono::milliseconds (10));
                }

            if (read_input)
                {
                    read_size = fread (buffer, 1, BUFFER_SIZE, input);
                    if (!(read_size > 0))
                        break;
                }

            // we don't know how much the resulting buffer is, so the best
            // reliable way with no optimization is to keep polling and
            // write/read byte by byte. polling will only tells whether an fd
            // can be operated on, we won't know if the write/read will be
            // blocked in the middle of operation (waiting for the exact
            // requested amount of space/data to be available) if we were to do
            // more than a byte at a time

            // we can do chunk writing here as as long as the output keeps
            // being read, the buffer will keep being consumed by ffmpeg, as
            // long as it's not too big
            int write_has_event = poll (pwfds, 1, 0);
            bool write_ready
                = (write_has_event > 0) && (pwfds[0].revents & POLLOUT);
            while (write_ready
                   && ((written_size += write (pwritefd, buffer + written_size,
                                               read_size - written_size))
                       < read_size))
                {
                    write_has_event = poll (pwfds, 1, 0);
                    write_ready = (write_has_event > 0)
                                  && (pwfds[0].revents & POLLOUT);
                }; // keep writing until buffer entirely written

            if (written_size < read_size)
                read_input = false;
            else
                {
                    read_input = true;
                    written_size = 0;
                }

            // poll ffmpeg stdout
            int read_has_event = poll (prfds, 1, 0);
            bool read_ready
                = (read_has_event > 0) && (prfds[0].revents & POLLIN);
            size_t input_read_size = 0;

            bool panic_break = false;

            if (read_ready)
                {
                    uint8_t out_b;
                    while (
                        read_ready
                        && ((input_read_size = read (preadfd, &out_b, 1)) > 0))
                        {
                            send_buffer[send_buffer_length] = out_b;
                            send_buffer_length += 1;

                            if (send_buffer_length == SEND_BUFFER_SIZE)
                                {
                                    if (!vclient || vclient->terminating)
                                        {
                                            panic_break = true;
                                            break;
                                        }
                                    vclient->send_audio_raw (
                                        (uint16_t *)send_buffer,
                                        send_buffer_length);

                                    send_buffer_length = 0;
                                }

                            read_has_event = poll (prfds, 1, 0);
                            read_ready = (read_has_event > 0)
                                         && (prfds[0].revents & POLLIN);
                        }
                }

            if (panic_break)
                break;

            // recreate ffmpeg process to update filter chain
            if ((float)p_track->player->volume != current_volume)
                {
                    // close write fd, we can now safely read until
                    // eof without worrying about polling and blocked read
                    close (pwritefd);

                    // read the rest of data before closing current instance
                    while ((input_read_size
                            = read (preadfd, send_buffer + send_buffer_length,
                                    SEND_BUFFER_SIZE - send_buffer_length))
                           > 0)
                        {
                            send_buffer_length += input_read_size;

                            if (send_buffer_length == SEND_BUFFER_SIZE)
                                {
                                    if (!vclient || vclient->terminating)
                                        {
                                            panic_break = true;
                                            break;
                                        }

                                    vclient->send_audio_raw (
                                        (uint16_t *)send_buffer,
                                        send_buffer_length);
                                    send_buffer_length = 0;
                                }
                        }

                    if (send_buffer_length > 0 && vclient
                        && !vclient->terminating)
                        {
                            vclient->send_audio_raw ((uint16_t *)send_buffer,
                                                     send_buffer_length);
                        }

                    // close read fd
                    close (preadfd);

                    pwritefd = -1;
                    preadfd = -1;

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
                            // fds will be closed on exit
                            perror ("cpipe");
                            error_status = ERR_LPIPE;
                            break;
                        }
                    creadfd = p_info->cpipefd[0];
                    pwritefd = p_info->cpipefd[1];

                    p_info->cpid = fork ();
                    if (p_info->cpid == -1)
                        {
                            // fds will be closed on exit
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

                    creadfd = -1;
                    cwritefd = -1;

                    // update fd to poll
                    prfds[0].fd = preadfd;
                    pwfds[0].fd = pwritefd;

                    // mark changes done
                    current_volume = (float)p_track->player->volume;
                }
        }

    if (send_buffer_length > 0 && vclient && !vclient->terminating)
        {
            vclient->send_audio_raw ((uint16_t *)send_buffer,
                                     send_buffer_length);
            send_buffer_length = 0;
        }

    // exiting, clean up
    fclose (input);
    input = NULL;

    if (creadfd > -1)
        close (creadfd);
    if (pwritefd > -1)
        close (pwritefd);
    if (cwritefd > -1)
        close (cwritefd);
    if (preadfd > -1)
        close (preadfd);

    creadfd = -1;
    pwritefd = -1;
    cwritefd = -1;
    preadfd = -1;

    if (debug)
        fprintf (stderr, "fds closed\n");

    // kill child
    kill (p_info->rpid, SIGTERM);
    kill (p_info->cpid, SIGTERM);

    // wait for childs to make sure they're dead and
    // prevent them to become zombies
    if (debug)
        fprintf (stderr, "waiting for child\n");
    waitpid (p_info->cpid, &cstatus, 0); /* Wait for child */
    if (debug)
        fprintf (stderr, "killed cchild status: %d\n", cstatus);
    waitpid (p_info->rpid, &cstatus, 0); /* Wait for child */
    if (debug)
        fprintf (stderr, "killed rchild status: %d\n", cstatus);
    assert (waitpid (p_info->cpid, &cstatus, 0) == -1);
    assert (waitpid (p_info->rpid, &cstatus, 0) == -1);

    return error_status;

    // init error handling
err_sfork:
    close (creadfd);
    close (pwritefd);

err_spipe2:
    close (preadfd);
    close (cwritefd);

err_spipe1:
    fclose (input);
    kill (p_info->rpid, SIGTERM);
    waitpid (p_info->rpid, &cstatus, 0);
    assert (waitpid (p_info->rpid, &cstatus, 0) == -1);

    return init_error;
} // run_processor

} // audio_processing
} // musicat
