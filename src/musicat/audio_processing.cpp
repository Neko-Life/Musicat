#include "musicat/audio_processing.h"
#include "musicat/child.h"
#include "musicat/child/command.h"
#include "musicat/musicat.h"
#include <assert.h>
#include <chrono>
#include <fcntl.h>
#include <iostream>
#include <poll.h>
#include <stdio.h>
#include <string>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

#define DPP_AUDIO_BUFFER_LENGTH_SECOND 0.3f
#define SLEEP_ON_BUFFER_THRESHOLD_MS 40

#define BUFFER_SIZE processing_buffer_size
#define READ_CHUNK_SIZE BUFSIZ / 2

#define USING_OPUS "-f", "opus", "-b:a", "128k"
#define USING_PCM "-f", "s16le"

#define USING_FORMAT USING_PCM

#define OUT_CMD "pipe:1"

namespace musicat
{
namespace audio_processing
{

int
read_command (processor_options_t &options)
{
    pollfd cmdrfds[1];
    cmdrfds[0].events = POLLIN;
    cmdrfds[0].fd = STDIN_FILENO;

    ssize_t read_cmd_size = 0;
    char cmd_buf[CMD_BUFSIZE + 1];

    int has_cmd = poll (cmdrfds, 1, 0);
    bool read_cmd = (has_cmd > 0) && (cmdrfds[0].revents & POLLIN);
    while (
        read_cmd
        && ((read_cmd_size = read (cmdrfds[0].fd, cmd_buf, CMD_BUFSIZE)) > 0))
        {
            cmd_buf[CMD_BUFSIZE] = '\0';

            if (options.debug)
                fprintf (stderr,
                         "[audio_processing::read_command %s] Processor "
                         "command: %s\n",
                         options.guild_id.c_str (), cmd_buf);

            const std::string cmd (cmd_buf);

            {
                using namespace child::command;
                command_options_t command_options = create_command_options ();
                parse_command_to_options (cmd, command_options);

                if (command_options.command == command_options_keys_t.seek)
                    {
                        options.seek_to = command_options.seek;
                    }

                else if (command_options.command
                         == command_options_keys_t.volume)
                    {
                        options.volume = command_options.volume;
                    }
            }

            has_cmd = poll (cmdrfds, 1, 0);
            read_cmd = (has_cmd > 0) && (cmdrfds[0].revents & POLLIN);
        }

    return 0;
}

static ssize_t
write_stdout (uint8_t *buffer, ssize_t *size, int write_fifo)
{
    ssize_t written = 0;
    ssize_t current_written = 0;
    while (((current_written
             = write (write_fifo, buffer + written, *size - written))
            > 0)
           || (written < *size))
        {
            if (current_written < 1)
                {
                    return -1;
                }

            written += current_written;
        };

    *size = 0;

    return written;
}
/*
static int
run_reader (const processor_options_t &options,
            const processor_states_t &p_info)
{
    // request kernel to kill little self when parent dies
    if (prctl (PR_SET_PDEATHSIG, SIGTERM) == -1)
        {
            perror ("child reader prctl");
            _exit (EXIT_FAILURE);
        }
    int prreadfd = p_info.rpipefd[0];
    int crwritefd = p_info.rpipefd[1];

    close (prreadfd);

    int status;
    status = dup2 (crwritefd, STDOUT_FILENO);
    close (crwritefd);
    if (status == -1)
        {
            perror ("child reader dout");
            _exit (EXIT_FAILURE);
        }

    // redirect ffmpeg stderr to /dev/null
    int dnull = open ("/dev/null", O_WRONLY);
    dup2 (dnull, STDERR_FILENO);
    close (dnull);

    const std::string file_path = options.file_path;

    const bool need_seek = options.seek_to.length () > 0;

    char *args[64] = {
        "ffmpeg",
    };
    int args_idx = 1;

    if (need_seek)
        {
            args[args_idx++] = "-ss";
            args[args_idx++] = (char *)options.seek_to.c_str ();
        }

    char *rest_args[] = { // "-v",
                          // "debug",
                          "-i",
                          (char *)file_path.c_str (),
                          "-f",
                          "f32le",
                          "-ac",
                          "2",
                          "-ar",
                          "48000",
                          //"-preset", "ultrafast",
                          "-threads",
                          "1",
                          OUT_CMD,
                          (char *)NULL
    };

    for (unsigned long i = 0; i < (sizeof (rest_args) / sizeof (rest_args[0]));
         i++)
        {
            args[args_idx++] = rest_args[i];
        }

    execvp ("ffmpeg", args);

    perror ("child reader exit");
    _exit (EXIT_FAILURE);
}

// p*fd = fd for parent
// c*fd = fd for child
static int
run_ffmpeg (processor_options_t &options, processor_states_t &p_info)
{
    // request kernel to kill little self when parent dies
    if (prctl (PR_SET_PDEATHSIG, SIGTERM) == -1)
        {
            perror ("child ffmpeg prctl");
            _exit (EXIT_FAILURE);
        }

    int preadfd = p_info.ppipefd[0];
    int cwritefd = p_info.ppipefd[1];
    int creadfd = p_info.cpipefd[0];
    int pwritefd = p_info.cpipefd[1];

    close (pwritefd);
    close (preadfd);

    // redirect ffmpeg stdin and stdout to prepared pipes
    // exit if unable to redirect
    int status;

    status = dup2 (creadfd, STDIN_FILENO);
    close (creadfd);
    if (status == -1)
        {
            perror ("child ffmpeg din");
            _exit (EXIT_FAILURE);
        }

    status = dup2 (cwritefd, STDOUT_FILENO);
    close (cwritefd);
    if (status == -1)
        {
            perror ("child ffmpeg dout");
            _exit (EXIT_FAILURE);
        }

    if (!options.debug)
        {
            // redirect ffmpeg stderr to /dev/null
            int dnull = open ("/dev/null", O_WRONLY);
            dup2 (dnull, STDERR_FILENO);
            close (dnull);
        }

    execlp ("ffmpeg", "ffmpeg", "-v", "debug", "-f", "f32le", "-ac", "2",
            "-ar", "48000", "-i", "pipe:0", "-af",
            (std::string ("volume=")
             + std::to_string ((float)options.volume / (float)100))
                .c_str (),
            "-ac", "2", "-ar", "48000", USING_FORMAT,
            //"-bufsize", "1024",
            //"-preset", "ultrafast",
            //"-threads", "1", OUT_CMD,
            (char *)NULL);

    perror ("child ffmpeg exit");
    _exit (EXIT_FAILURE);
}
*/
// this should be called
// inside the streaming thread
int
send_audio_routine (dpp::discord_voice_client *vclient, uint16_t *send_buffer,
                    ssize_t *send_buffer_length, bool no_wait)
{
    const bool debug = get_debug_state ();
    bool running_state = get_running_state ();
    // wait for old pending audio buffer to be sent to discord
    if (!no_wait)
        {
            float outbuf_duration;

            while ((running_state = get_running_state ()) && vclient
                   && !vclient->terminating
                   && ((outbuf_duration = vclient->get_secs_remaining ())
                       > DPP_AUDIO_BUFFER_LENGTH_SECOND))
                {
                    if (debug)
                        {
                            static std::chrono::time_point start_time
                                = std::chrono::high_resolution_clock::now ();

                            auto end_time
                                = std::chrono::high_resolution_clock::now ();

                            auto done = std::chrono::duration_cast<
                                std::chrono::milliseconds> (end_time
                                                            - start_time);

                            start_time = end_time;

                            fprintf (stderr,
                                     "[audio_processing::send_audio_routine] "
                                     "outbuf_duration: %f > %f\n",
                                     outbuf_duration,
                                     DPP_AUDIO_BUFFER_LENGTH_SECOND);

                            fprintf (
                                stderr,
                                "[audio_processing::send_audio_routine] Delay "
                                "between send: %ld milliseconds\n",
                                done.count ());
                        }

                    std::this_thread::sleep_for (std::chrono::milliseconds (
                        SLEEP_ON_BUFFER_THRESHOLD_MS));
                }
        }

    if (!running_state || !vclient || vclient->terminating)
        {
            return 1;
        }

    try
        {
            vclient->send_audio_raw (send_buffer, *send_buffer_length);
        }
    catch (const dpp::voice_exception &e)
        {
            fprintf (stderr,
                     "[audio_processing::send_audio_routine ERROR] %s\n",
                     e.what ());
        }

    *send_buffer_length = 0;

    return 0;
}

processor_options_t
create_options ()
{
    return { "", false, false, "", 100, "", "" };
}

processor_options_t
copy_options (processor_options_t &opts)
{
    return opts;
}
/*
// should be run as a child process
run_processor_error_t
run_processor_2_child (child::command::command_options_t &process_options)
{
    // !TODO: remove this redirect
    // int dnull = open ("/dev/null", O_WRONLY);
    // dup2 (dnull, STDOUT_FILENO);
    // close (dnull);

    processor_states_t p_info;

    processor_options_t options = create_options ();
    options.file_path = process_options.file_path;
    options.debug = process_options.debug;
    options.id = process_options.id;
    options.guild_id = process_options.guild_id;
    options.volume = process_options.volume;

    run_processor_error_t init_error = SUCCESS;
    run_processor_error_t error_status = SUCCESS;

    // decode opus track
    if (pipe (p_info.rpipefd) == -1)
        {
            perror ("rpipe");
            return ERR_INPUT;
        }
    int prreadfd = p_info.rpipefd[0];
    int crwritefd = p_info.rpipefd[1];

    p_info.rpid = fork ();
    if (p_info.rpid == -1)
        {
            perror ("rfork");

            close (prreadfd);
            close (crwritefd);

            return ERR_INPUT;
        }

    if (p_info.rpid == 0)
        {
            run_reader (options, p_info);
        }

    close (crwritefd);
    crwritefd = -1;

    // when closing, assign NULL and prreadfd=-1
    // closing this will also close prreadfd
    FILE *input = fdopen (prreadfd, "r");

    // declare everything here to be able to use goto
    // fds and poll
    int preadfd, cwritefd, creadfd, pwritefd, cstatus = 0;
    struct pollfd prfds[1], pwfds[1];

    // main loop variable definition
    ssize_t read_size = 0;
    uint8_t buffer[BUFFER_SIZE];
    ssize_t last_read_size = 0;
    uint8_t rest_buffer[BUFFER_SIZE];
    bool read_input = true;
    ssize_t written_size = 0;

    processor_options_t current_options = copy_options (options);

    int write_fifo
        = open (process_options.audio_stream_fifo_path.c_str (), O_WRONLY),
        stdin_fifo, fifo_status, stdout_fifo;

    if (write_fifo < 0)
        {
            perror ("write_fifo");
            init_error = ERR_SFIFO;
            goto err_sfifo1;
        }

    stdin_fifo
        = open (process_options.audio_stream_stdin_path.c_str (), O_RDONLY);

    if (stdin_fifo < 0)
        {
            perror ("stdin_fifo");
            init_error = ERR_SFIFO;
            goto err_sfifo2;
        }

    fifo_status = dup2 (stdin_fifo, STDIN_FILENO);
    close (stdin_fifo);
    if (fifo_status == -1)
        {
            perror ("dup2 stdin_fifo");
            init_error = ERR_SFIFO;
            goto err_sfifo3;
        }

    stdout_fifo
        = open (process_options.audio_stream_stdout_path.c_str (), O_WRONLY);

    if (stdout_fifo < 0)
        {
            perror ("stdout_fifo");
            init_error = ERR_SFIFO;
            goto err_sfifo4;
        }

    fifo_status = dup2 (stdout_fifo, STDOUT_FILENO);
    close (stdout_fifo);
    if (fifo_status == -1)
        {
            perror ("dup2 stdout_fifo");
            init_error = ERR_SFIFO;
            goto err_sfifo5;
        }

    // prepare required pipes for bidirectional interprocess communication
    if (pipe (p_info.ppipefd) == -1)
        {
            perror ("ppipe");
            init_error = ERR_SPIPE;
            goto err_spipe1;
        }
    preadfd = p_info.ppipefd[0];
    cwritefd = p_info.ppipefd[1];

    if (pipe (p_info.cpipefd) == -1)
        {
            perror ("cpipe");
            init_error = ERR_SPIPE;
            goto err_spipe2;
        }
    creadfd = p_info.cpipefd[0];
    pwritefd = p_info.cpipefd[1];

    // create a child
    p_info.cpid = fork ();
    if (p_info.cpid == -1)
        {
            perror ("fork");
            init_error = ERR_SFORK;
            goto err_sfork;
        }

    if (p_info.cpid == 0)
        {
            // close unrelated fds
            fclose (input);
            close (write_fifo);

            run_ffmpeg (options, p_info);
        }

    close (creadfd);
    close (cwritefd);

    creadfd = -1;
    cwritefd = -1;

    // prepare required data for polling
    prfds[0].events = POLLIN;
    pwfds[0].events = POLLOUT;

    prfds[0].fd = preadfd;
    pwfds[0].fd = pwritefd;

    // main loop, breaking means exiting
    while (!options.panic_break)
        {
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
            //
            // as safe chunk read size is known we no longer need to read byte
            // by byte

            // we can do chunk writing here as as long as the output keeps
            // being read, the buffer will keep being consumed by ffmpeg, as
            // long as it's not too big it won't block to wait for buffer space
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

                    if (pwfds[0].revents & POLLERR)
                        break;
                }; // keep writing until buffer entirely written

            if (written_size < read_size)
                {
                    if (pwfds[0].revents & POLLERR)
                        break;

                    read_input = false;
                }
            else
                {
                    read_input = true;
                    written_size = 0;
                }

            // poll ffmpeg stdout
            int read_has_event = poll (prfds, 1, 0);
            bool read_ready
                = (read_has_event > 0) && (prfds[0].revents & POLLIN);

            ssize_t input_read_size = 0;

            if (read_ready)
                {
                    // with known chunk size that always guarantee correct read
                    // size that empties ffmpeg buffer, poll will report
                    // correctly that the pipe is really empty so we know
                    // exactly when to stop reading, this is where it's
                    // problematic when BUFFER_SIZE%READ_CHUNK_SIZE doesn't
                    // equal 0
                    uint8_t out_buffer[BUFFER_SIZE];
                    while (read_ready
                           && ((input_read_size
                                += read (preadfd, out_buffer + input_read_size,
                                         READ_CHUNK_SIZE))
                               > 0))
                        {
                            if (input_read_size == BUFFER_SIZE)
                                {
                                    if (write_stdout (out_buffer,
                                                      &input_read_size,
                                                      write_fifo)
                                        == -1)
                                        {
                                            options.panic_break = true;
                                            break;
                                        }
                                }

                            read_has_event = poll (prfds, 1, 0);
                            read_ready = (read_has_event > 0)
                                         && (prfds[0].revents & POLLIN);
                        }

                    // empties the last buffer that usually size less than
                    // BUFFER_SIZE which usually left with 0 remainder when
                    // divided by READ_CHUNK_SIZE, otherwise can means ffmpeg
                    // have chunked audio packets with its specific size as its
                    // output when specifying some other containerized format
                    if (input_read_size > 0)
                        {
                            if (write_stdout (out_buffer, &input_read_size,
                                              write_fifo)
                                == -1)
                                {
                                    options.panic_break = true;
                                    break;
                                };
                        }
                }

            // !TODO: poll and read for commands from stdin
            // commands: volume, panic_break
            // probably create a big struct for every possible options and one
            // universal command parser that will set those options
            // commands should always be the length of CMD_BUFSIZE
            read_command (options);
            if (options.panic_break)
                break;

            if (options.seek_to.length ())
                {
                    // restart reader to apply seek
                    if (input)
                        {
                            fclose (input);
                            prreadfd = -1;
                            input = NULL;
                        }

                    cstatus = 0;

                    // kill reader immediately as closing its stdout or SIGTERM
                    // won't trigger it to exit
                    // kill (p_info.rpid, SIGKILL);

                    waitpid (p_info.rpid, &cstatus, 0);
                    if (options.debug)
                        fprintf (stderr, "rchild status: %d\n", cstatus);

                    // char notif[CMD_BUFSIZE];
                    // memset (notif, '\0', sizeof (notif) / sizeof
                    // (notif[0]));

                    if (pipe (p_info.rpipefd) == -1)
                        {
                            perror ("rpipe");
                            // notif[0] = '1';
                            // write (STDOUT_FILENO, notif, CMD_BUFSIZE);
                            break;
                        }
                    prreadfd = p_info.rpipefd[0];
                    crwritefd = p_info.rpipefd[1];

                    p_info.rpid = fork ();
                    if (p_info.rpid == -1)
                        {
                            perror ("rfork");

                            close (prreadfd);
                            close (crwritefd);

                            // notif[0] = '2';
                            // write (STDOUT_FILENO, notif, CMD_BUFSIZE);
                            break;
                        }

                    if (p_info.rpid == 0)
                        {
                            // close unrelated fds to avoid deadlock
                            close (pwritefd);
                            close (preadfd);
                            close (write_fifo);

                            run_reader (options, p_info);
                        }

                    close (crwritefd);
                    crwritefd = -1;

                    input = fdopen (prreadfd, "r");

                    options.seek_to = "";

                    // notif[0] = '0';
                    // write (STDOUT_FILENO, notif, CMD_BUFSIZE);
                }

            // recreate ffmpeg process to update filter chain
            if (options.volume != current_options.volume)
                {
                    close (pwritefd);

                    // read the rest of data before closing current instance
                    // read_has_event = poll (prfds, 1, 0);
                    // read_ready = (read_has_event > 0)
                    //              && (prfds[0].revents & POLLIN
                    //                  && !(prfds[0].revents & POLLERR));
                    //
                    while (//read_ready
                           //&&
                           (input_read_size
                            = read (preadfd, rest_buffer, BUFFER_SIZE))
                           > 0)
                        {
                            if (write_stdout (rest_buffer, &input_read_size,
                                              write_fifo)
                                == -1)
                                {
                                    options.panic_break = true;
                                    break;
                                };


                            // read_has_event = poll (prfds, 1, 0);
                            // read_ready = (read_has_event > 0)
                            //              && (prfds[0].revents & POLLIN
                            //                  && !(prfds[0].revents &
POLLERR));
                        }

                    // close read fd
                    close (preadfd);

                    pwritefd = -1;
                    preadfd = -1;

                    cstatus = 0;

                    // kill ffmpeg immediately as closing its stdout or SIGTERM
                    // won't trigger it to exit
                    // kill (p_info.cpid, SIGKILL);

                    // wait for child to finish transferring data
                    waitpid (p_info.cpid, &cstatus, 0);
                    if (options.debug)
                        fprintf (stderr, "child status: %d\n", cstatus);

                    // do the same setup routine as startup
                    if (pipe (p_info.ppipefd) == -1)
                        {
                            perror ("ppipe");
                            error_status = ERR_LPIPE;
                            break;
                        }
                    preadfd = p_info.ppipefd[0];
                    cwritefd = p_info.ppipefd[1];

                    if (pipe (p_info.cpipefd) == -1)
                        {
                            perror ("cpipe");
                            error_status = ERR_LPIPE;

                            close (preadfd);
                            close (cwritefd);

                            preadfd = -1;
                            cwritefd = -1;

                            break;
                        }
                    creadfd = p_info.cpipefd[0];
                    pwritefd = p_info.cpipefd[1];

                    p_info.cpid = fork ();
                    if (p_info.cpid == -1)
                        {
                            perror ("fork");
                            error_status = ERR_LFORK;

                            close (preadfd);
                            close (cwritefd);
                            close (pwritefd);
                            close (creadfd);

                            preadfd = -1;
                            cwritefd = -1;
                            creadfd = -1;
                            pwritefd = -1;

                            break;
                        }

                    if (p_info.cpid == 0)
                        {
                            // close unrelated fds to avoid deadlock
                            fclose (input);
                            close (write_fifo);

                            run_ffmpeg (options, p_info);
                        }

                    close (creadfd);
                    close (cwritefd);

                    creadfd = -1;
                    cwritefd = -1;

                    // update fd to poll
                    prfds[0].fd = preadfd;
                    pwfds[0].fd = pwritefd;

                    // mark changes done
                    current_options.volume = options.volume;
                }
        }

    close (process_options.child_write_fd);
    process_options.child_write_fd = -1;

    // exiting, clean up
    if (input)
        {
            fclose (input);
            prreadfd = -1;
            input = NULL;
        }

    if (pwritefd > -1)
        close (pwritefd);

    // read the rest of data before closing ffmpeg stdout
    while ((last_read_size = read (preadfd, rest_buffer, BUFFER_SIZE)) > 0)
        {
            if (write_stdout (rest_buffer, &last_read_size, write_fifo) == -1)
                {
                    options.panic_break = true;
                    break;
                };
        }

    if (preadfd > -1)
        close (preadfd);

    close (write_fifo);

    pwritefd = -1;
    preadfd = -1;
    write_fifo = -1;

    if (options.debug)
        fprintf (stderr, "fds closed\n");

    // wait for childs to make sure they're dead and
    // prevent them to become zombies
    if (options.debug)
        fprintf (stderr, "waiting for child\n");

    cstatus = 0;

    waitpid (p_info.cpid, &cstatus, 0);
    if (options.debug)
        fprintf (stderr, "cchild status: %d\n", cstatus);

    cstatus = 0;

    waitpid (p_info.rpid, &cstatus, 0);
    if (options.debug)
        fprintf (stderr, "rchild status: %d\n", cstatus);

    return error_status;

    // init error handling
err_sfork:
    close (creadfd);
    close (pwritefd);
err_spipe2:
    close (preadfd);
    close (cwritefd);
err_spipe1:
err_sfifo5:
err_sfifo4:
err_sfifo3:
err_sfifo2:
    close (write_fifo);
err_sfifo1:
    close (process_options.child_write_fd);
    process_options.child_write_fd = -1;

    fclose (input);
    waitpid (p_info.rpid, &cstatus, 0);

    return init_error;
} // run_processor_2_child
*/
static int
run_standalone (const processor_options_t &options,
                const processor_states_t &p_info)
{
    // request kernel to kill little self when parent dies
    if (prctl (PR_SET_PDEATHSIG, SIGTERM) == -1)
        {
            perror ("child standalone prctl");
            _exit (EXIT_FAILURE);
        }
    int preadfd = p_info.ppipefd[0];
    int cwritefd = p_info.ppipefd[1];
    int creadfd = p_info.cpipefd[0];
    int pwritefd = p_info.cpipefd[1];

    close (preadfd);  /* Close unused read end */
    close (pwritefd); /* Close unused read end */

    int status;
    status = dup2 (cwritefd, STDOUT_FILENO);
    close (cwritefd);
    if (status == -1)
        {
            perror ("child standalone dout");
            _exit (EXIT_FAILURE);
        }

    status = dup2 (creadfd, STDIN_FILENO);
    close (creadfd);
    if (status == -1)
        {
            perror ("child standalone din");
            _exit (EXIT_FAILURE);
        }

    // redirect ffmpeg stderr to /dev/null
    if (!options.debug)
        {
            // redirect ffmpeg stderr to /dev/null
            int dnull = open ("/dev/null", O_WRONLY);
            dup2 (dnull, STDERR_FILENO);
            close (dnull);
        }

    const std::string file_path = options.file_path;

    const bool need_seek = options.seek_to.length () > 0;

    char *args[64] = {
        "ffmpeg",
    };
    int args_idx = 1;

    if (need_seek)
        {
            args[args_idx++] = "-ss";
            args[args_idx++] = (char *)options.seek_to.c_str ();
        }

    char *rest_args[]
        = { "-v",
            "debug",
            "-i",
            (char *)file_path.c_str (),
            "-af",
            (char *)(std::string ("volume=")
                     + std::to_string ((float)options.volume / (float)100))
                .c_str (),
            "-ac",
            "2",
            "-ar",
            "48000",
            /*"-preset", "ultrafast",*/ "-threads",
            "1",
            USING_FORMAT,
            OUT_CMD,
            (char *)NULL };

    for (unsigned long i = 0; i < (sizeof (rest_args) / sizeof (rest_args[0]));
         i++)
        args[args_idx++] = rest_args[i];

    if (options.debug)
        for (unsigned long i = 0; i < (sizeof (args) / sizeof (args[0])); i++)
            {
                if (!args[i])
                    break;

                fprintf (stderr, "%s\n", args[i]);
            }

    execvp ("ffmpeg", args);

    perror ("child standalone exit");
    _exit (EXIT_FAILURE);
}

// should be run as a child process
run_processor_error_t
run_processor (child::command::command_options_t &process_options)
{
    // !TODO: remove this useless child_write_fd entirely
    close (process_options.child_write_fd);
    process_options.child_write_fd = -1;

    processor_states_t p_info;

    processor_options_t options = create_options ();
    options.file_path = process_options.file_path;
    options.debug = process_options.debug;
    options.id = process_options.id;
    options.guild_id = process_options.guild_id;
    options.volume = process_options.volume;

    run_processor_error_t init_error = SUCCESS;
    run_processor_error_t error_status = SUCCESS;

    // declare everything here to be able to use goto
    // fds and poll
    int preadfd, cwritefd, creadfd, pwritefd, cstatus = 0;
    struct pollfd prfds[1], pwfds[1];

    // main loop variable definition
    ssize_t last_read_size = 0;
    uint8_t rest_buffer[BUFFER_SIZE];

    processor_options_t current_options = copy_options (options);

    int write_fifo
        = open (process_options.audio_stream_fifo_path.c_str (), O_WRONLY),
        stdin_fifo, fifo_status, stdout_fifo;

    if (write_fifo < 0)
        {
            perror ("write_fifo");
            init_error = ERR_SFIFO;
            goto err_sfifo1;
        }

    stdin_fifo
        = open (process_options.audio_stream_stdin_path.c_str (), O_RDONLY);

    if (stdin_fifo < 0)
        {
            perror ("stdin_fifo");
            init_error = ERR_SFIFO;
            goto err_sfifo2;
        }

    fifo_status = dup2 (stdin_fifo, STDIN_FILENO);
    close (stdin_fifo);
    if (fifo_status == -1)
        {
            perror ("dup2 stdin_fifo");
            init_error = ERR_SFIFO;
            goto err_sfifo3;
        }

    stdout_fifo
        = open (process_options.audio_stream_stdout_path.c_str (), O_WRONLY);

    if (stdout_fifo < 0)
        {
            perror ("stdout_fifo");
            init_error = ERR_SFIFO;
            goto err_sfifo4;
        }

    fifo_status = dup2 (stdout_fifo, STDOUT_FILENO);
    close (stdout_fifo);
    if (fifo_status == -1)
        {
            perror ("dup2 stdout_fifo");
            init_error = ERR_SFIFO;
            goto err_sfifo5;
        }

    // prepare required pipes for bidirectional interprocess communication
    if (pipe (p_info.ppipefd) == -1)
        {
            perror ("ppipe");
            init_error = ERR_SPIPE;
            goto err_spipe1;
        }
    preadfd = p_info.ppipefd[0];
    cwritefd = p_info.ppipefd[1];

    if (pipe (p_info.cpipefd) == -1)
        {
            perror ("cpipe");
            init_error = ERR_SPIPE;
            goto err_spipe2;
        }
    creadfd = p_info.cpipefd[0];
    pwritefd = p_info.cpipefd[1];

    // create a child
    p_info.cpid = fork ();
    if (p_info.cpid == -1)
        {
            perror ("fork");
            init_error = ERR_SFORK;
            goto err_sfork;
        }

    if (p_info.cpid == 0)
        {
            // close unrelated fds
            close (write_fifo);

            run_standalone (options, p_info);
        }

    close (cwritefd); /* Close unused write end */
    close (creadfd);  /* Close unused write end */

    cwritefd = -1;

    // prepare required data for polling
    prfds[0].events = POLLIN;
    pwfds[0].events = POLLOUT;

    prfds[0].fd = preadfd;
    pwfds[0].fd = pwritefd;

    // main loop, breaking means exiting
    while (!options.panic_break)
        {
            // poll ffmpeg stdout
            int read_has_event = poll (prfds, 1, 0);
            bool read_ready
                = (read_has_event > 0) && (prfds[0].revents & POLLIN);

            ssize_t input_read_size = 0;

            if (read_ready)
                {
                    // with known chunk size that always guarantee correct read
                    // size that empties ffmpeg buffer, poll will report
                    // correctly that the pipe is really empty so we know
                    // exactly when to stop reading, this is where it's
                    // problematic when BUFFER_SIZE%READ_CHUNK_SIZE doesn't
                    // equal 0
                    uint8_t out_buffer[BUFFER_SIZE];
                    ssize_t current_read = 0;
                    while (read_ready
                           && ((current_read
                                = read (preadfd, out_buffer + input_read_size,
                                        READ_CHUNK_SIZE))
                               > 0))
                        {
                            input_read_size += current_read;
                            if (input_read_size == BUFFER_SIZE)
                                {
                                    if (write_stdout (out_buffer,
                                                      &input_read_size,
                                                      write_fifo)
                                        == -1)
                                        {
                                            options.panic_break = true;
                                            break;
                                        }
                                }

                            // break on last buffer
                            if (current_read < READ_CHUNK_SIZE)
                                {
                                    break;
                                }

                            read_has_event = poll (prfds, 1, 0);
                            read_ready = (read_has_event > 0)
                                         && (prfds[0].revents & POLLIN);
                        }

                    // empties the last buffer that usually size less than
                    // BUFFER_SIZE which usually left with 0 remainder when
                    // divided by READ_CHUNK_SIZE, otherwise can means ffmpeg
                    // have chunked audio packets with its specific size as its
                    // output when specifying some other containerized format
                    if (input_read_size > 0)
                        {
                            if (write_stdout (out_buffer, &input_read_size,
                                              write_fifo)
                                == -1)
                                {
                                    options.panic_break = true;
                                    break;
                                };
                        }
                }
            else if ((prfds[0].revents & POLLERR)
                     || (prfds[0].revents & POLLHUP))
                {
                    // we got doomed
                    perror ("main poll");
                    break;
                }

            // !TODO: poll and read for commands from stdin
            // commands: volume, panic_break
            // probably create a big struct for every possible options and one
            // universal command parser that will set those options
            // commands should always be the length of CMD_BUFSIZE
            read_command (options);
            if (options.panic_break)
                break;

            // recreate ffmpeg process to update filter chain
            if (options.seek_to.length ())
                {
                    close (pwritefd);

                    kill (p_info.cpid, SIGTERM);

                    // read the rest of data before closing current instance
                    // have some patient and wait for 1000 ms each poll
                    // cuz if the pipe isn't really empty by the time
                    // read fd closed (cuz late data), the to be created new
                    // pipe will have POLLHUP and can't be recovered
                    read_has_event = poll (prfds, 1, 1000);
                    read_ready
                        = (read_has_event > 0) && (prfds[0].revents & POLLIN);
                    while (read_ready
                           && (input_read_size
                               = read (preadfd, rest_buffer, BUFFER_SIZE))
                                  > 0)
                        {
                            if (write_stdout (rest_buffer, &input_read_size,
                                              write_fifo)
                                == -1)
                                {
                                    options.panic_break = true;
                                    break;
                                };

                            read_has_event = poll (prfds, 1, 1000);
                            read_ready = (read_has_event > 0)
                                         && (prfds[0].revents & POLLIN);
                        }

                    // close read fd
                    close (preadfd);

                    preadfd = -1;

                    cstatus = 0;

                    // wait for child to finish transferring data
                    waitpid (p_info.cpid, &cstatus, 0);
                    if (options.debug)
                        fprintf (stderr, "child status: %d\n", cstatus);

                    // do the same setup routine as startup
                    if (pipe (p_info.ppipefd) == -1)
                        {
                            perror ("ppipe");
                            error_status = ERR_LPIPE;
                            break;
                        }
                    preadfd = p_info.ppipefd[0];
                    cwritefd = p_info.ppipefd[1];

                    if (pipe (p_info.cpipefd) == -1)
                        {
                            perror ("cpipe");
                            init_error = ERR_SPIPE;

                            close (preadfd);
                            close (cwritefd);

                            preadfd = -1;
                            cwritefd = -1;
                            break;
                        }
                    creadfd = p_info.cpipefd[0];
                    pwritefd = p_info.cpipefd[1];

                    p_info.cpid = fork ();
                    if (p_info.cpid == -1)
                        {
                            perror ("fork");
                            error_status = ERR_LFORK;

                            close (preadfd);
                            close (cwritefd);
                            close (pwritefd);
                            close (creadfd);

                            preadfd = -1;
                            cwritefd = -1;
                            pwritefd = -1;
                            creadfd = -1;

                            break;
                        }

                    if (p_info.cpid == 0)
                        {
                            // close unrelated fds to avoid deadlock
                            close (write_fifo);

                            run_standalone (options, p_info);
                        }

                    close (cwritefd); /* Close unused write end */
                    close (creadfd);  /* Close unused write end */

                    cwritefd = -1;
                    creadfd = -1;

                    // update fd to poll
                    prfds[0].fd = preadfd;
                    pwfds[0].fd = pwritefd;

                    // mark changes done
                    options.seek_to = "";
                }

            // runtime effects here
            if (options.volume != current_options.volume)
                {
                    const std::string str_buf
                        = "call -1 volume "
                          + std::to_string ((float)options.volume / (float)100)
                          + '\n';
                    const char *buf = str_buf.c_str ();

                    write (pwritefd, buf, str_buf.size () + 1);
                    current_options.volume = options.volume;
                }
        }

    // exiting, clean up

    // read the rest of data before closing ffmpeg stdout
    while ((last_read_size = read (preadfd, rest_buffer, BUFFER_SIZE)) > 0)
        {
            if (write_stdout (rest_buffer, &last_read_size, write_fifo) == -1)
                {
                    options.panic_break = true;
                    break;
                };
        }

    if (preadfd > -1)
        close (preadfd);

    close (write_fifo);

    preadfd = -1;
    write_fifo = -1;

    if (options.debug)
        fprintf (stderr, "fds closed\n");

    // wait for childs to make sure they're dead and
    // prevent them to become zombies
    if (options.debug)
        fprintf (stderr, "waiting for child\n");

    cstatus = 0;

    waitpid (p_info.cpid, &cstatus, 0); /* Wait for child */
    if (options.debug)
        fprintf (stderr, "cchild status: %d\n", cstatus);

    return error_status;

    // init error handling
err_sfork:
    close (pwritefd);
    close (creadfd);
err_spipe2:
    close (preadfd);
    close (cwritefd);
err_spipe1:
err_sfifo5:
err_sfifo4:
err_sfifo3:
err_sfifo2:
    close (write_fifo);
err_sfifo1:
    close (process_options.child_write_fd);
    process_options.child_write_fd = -1;

    return init_error;
} // run_processor

std::string
get_audio_stream_fifo_path (const std::string &id)
{
    return std::string ("/tmp/musicat.") + id + ".audio_stream";
}

mode_t
get_audio_stream_fifo_mode_t ()
{
    return 0600;
}

std::string
get_audio_stream_stdin_path (const std::string &id)
{
    return std::string ("/tmp/musicat.") + id + ".stdin";
}

std::string
get_audio_stream_stdout_path (const std::string &id)
{
    return std::string ("/tmp/musicat.") + id + ".stdout";
}

} // audio_processing
} // musicat
