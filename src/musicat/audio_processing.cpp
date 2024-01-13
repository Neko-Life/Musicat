#include "musicat/audio_processing.h"
#include "musicat/child.h"
#include "musicat/child/command.h"
#include "musicat/helper_processor.h"
#include "musicat/mctrack.h"
#include "musicat/musicat.h"
#include "musicat/util.h"
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

namespace musicat::audio_processing
{

// need to make this global to close it on worker fork
// processor audio stream out
int write_fifo = -1,
    // ffmpeg stdout
    preadfd = -1,
    // ffmpeg runtime command/stdin
    pwritefd = -1;

// whether current instance have notified parent that it's ready
// through its stdout
bool notified = false;

inline constexpr const char audio_cmd_str[]
    = "[audio_processing::read_command ";
inline constexpr const size_t audio_cmd_str_size
    = (sizeof (audio_cmd_str) / sizeof (*audio_cmd_str));

inline constexpr const char *audio_cmd_str2 = "%s] Processor command: %s\n";

// returns -1 if no command read, 0 if any
int
read_command (processor_options_t &options)
{
    pollfd cmdrfds[1];
    cmdrfds[0].events = POLLIN;
    cmdrfds[0].fd = STDIN_FILENO;

    ssize_t read_cmd_size = 0;
    char cmd_buf[CMD_BUFSIZE + 1];

    int status = -1;

    int has_cmd = poll (cmdrfds, 1, 0);
    bool read_cmd = (has_cmd > 0) && (cmdrfds[0].revents & POLLIN);

    // one command at a time, no while loop
    if (read_cmd
        && ((read_cmd_size = read (cmdrfds[0].fd, cmd_buf, CMD_BUFSIZE)) > 0))
        {
            status = 0;
            cmd_buf[CMD_BUFSIZE] = '\0';

            if (options.debug)
                {
                    write (STDERR_FILENO, audio_cmd_str, audio_cmd_str_size);

                    fprintf (stderr, audio_cmd_str2, options.guild_id.c_str (),
                             cmd_buf);
                }

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
                else if (command_options.command
                         == command_options_keys_t.helper_chain)
                    {
                        options.helper_chain.clear ();
                        parse_helper_chain_option (command_options, options);
                    }
            }
        }

    return status;
}

inline constexpr const char *idfmt
    = "[audio_processing::write_stdout] size, will go to chain: %ld %d\n";
inline constexpr const char *necdfmt
    = "[audio_processing::write_stdout] size after chain: %ld\n";

ssize_t
write_stdout (uint8_t *buffer, ssize_t *size, bool no_effect_chain)
{
    // this is wrong but whatever for now
    // bool debug = get_debug_state ();

    // if (debug)
    //     fprintf (stderr, idfmt, *size, !no_effect_chain);

    if (!no_effect_chain)
        {
            helper_processor::run_through_chain (buffer, size);

            /* if (debug) */
            // fprintf (stderr, necdfmt, *size);
        }

    if (*size == 0)
        return 0;

    if (!notified)
        {
            child::command::write_command ("0", STDOUT_FILENO,
                                           "audio_processing::write_stdout");

            notified = true;
        }

    ssize_t written = 0;
    ssize_t current_written = 0;
    while (((current_written
             = write (write_fifo, buffer + written, *size - written))
            > 0)
           || (written < *size))
        {
            if (current_written < 0)
                {
                    return -1;
                }

            written += current_written;
        };

    *size = 0;

    return written;
}

// this should be called
// inside the streaming thread
int
send_audio_routine (dpp::discord_voice_client *vclient, uint16_t *send_buffer,
                    ssize_t *send_buffer_length, bool no_wait)
{
    // const bool debug = get_debug_state ();
    bool running_state = get_running_state ();

    if (!running_state || !vclient || vclient->terminating)
        {
            return 1;
        }

    // calculate duration
    if ((*send_buffer_length > 0))
        {
            auto player_manager = get_player_manager_ptr ();
            auto guild_player
                = player_manager
                      ? player_manager->get_player (vclient->server_id)
                      : NULL;

            if (guild_player && guild_player->current_track.filesize)
                {
                    uint64_t duration
                        = mctrack::get_duration (guild_player->current_track);

                    float byte_per_ms
                        = (float)guild_player->current_track.filesize
                          / (float)duration;

                    int64_t samp_calc = guild_player->sampling_rate == -1
                                            ? 48000
                                            : guild_player->sampling_rate;

                    // take account earwax resampling
                    if (guild_player->earwax)
                        samp_calc -= 3900;

                    guild_player->current_track.current_byte
                        // (buffer_size /
                        // (sampling rate * channel *
                        // (bit width(16) / bit per byte(8))
                        // ) * 1 second in ms)
                        // * opus byte_per_ms
                        += (int64_t)((double)((float)((float)*send_buffer_length
                                                      / (samp_calc * 2 * 2)
                                                      * 1000)
                                              * byte_per_ms)
                                     * guild_player->tempo);
                }
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

static void
notify_seek_done ()
{
    write (write_fifo, "BOOB", 4);
}

processor_options_t
create_options ()
{
    return { "", false, false, "", 100, "", "", {} };
}

processor_options_t
copy_options (processor_options_t &opts)
{
    return opts;
}

// close all unrelated fds when helper got forked
static void
handle_worker_fork ()
{
    close_valid_fd (&write_fifo);

    helper_processor::close_all_helper_fds ();
}

static void
handle_helper_fork ()
{
    handle_worker_fork ();
    // close parent fds in addition
    close_valid_fd (&preadfd);
    close_valid_fd (&pwritefd);
}

static int
run_standalone (const processor_options_t &options,
                const processor_states_t &p_info, sem_t *sem)
{
    // request kernel to kill little self when parent dies
    if (prctl (PR_SET_PDEATHSIG, SIGTERM) == -1)
        {
            perror ("child standalone prctl");
            do_sem_post (sem);
            _exit (EXIT_FAILURE);
        }
    preadfd = p_info.ppipefd[0];
    int cwritefd = p_info.ppipefd[1];
    int creadfd = p_info.cpipefd[0];
    pwritefd = p_info.cpipefd[1];

    close (preadfd);  /* Close unused read end */
    close (pwritefd); /* Close unused read end */

    int status;
    status = dup2 (cwritefd, STDOUT_FILENO);
    close (cwritefd);
    if (status == -1)
        {
            perror ("child standalone dout");
            do_sem_post (sem);
            _exit (EXIT_FAILURE);
        }

    status = dup2 (creadfd, STDIN_FILENO);
    close (creadfd);
    if (status == -1)
        {
            perror ("child standalone din");
            do_sem_post (sem);
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

    const bool need_seek = !options.seek_to.empty ();

    char *args[64] = {
        "ffmpeg",
    };
    int args_idx = 1;

    if (need_seek)
        {
            args[args_idx++] = "-ss";
            args[args_idx++] = (char *)options.seek_to.c_str ();
        }

    std::string vol_arg
        = "volume=" + std::to_string ((float)options.volume / (float)100);

    char *rest_args[] = { "-v",
                          "debug",
#ifdef FFMPEG_REALTIME
                          "-probesize",
                          "32",
#endif
                          "-i",
                          (char *)file_path.c_str (),
                          "-af",
                          (char *)vol_arg.c_str (),
                          "-ac",
                          "2",
                          "-ar",
                          "48000",
                          /*"-preset", "ultrafast",*/ "-threads",
                          "1",
                          "-stdin",
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

    do_sem_post (sem);
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
    int cwritefd, creadfd, cstatus = 0;
    struct pollfd prfds[1], pwfds[1];

    // main loop variable definition
    ssize_t last_read_size = 0;
    uint8_t rest_buffer[BUFFER_SIZE];

    processor_options_t current_options = copy_options (options);
    parse_helper_chain_option (process_options, options);

    helper_processor::manage_processor (options, handle_helper_fork);

    int stdin_fifo, fifo_status, stdout_fifo;

    sem_t *sem;
    std::string sem_full_key;

    write_fifo
        = open (process_options.audio_stream_fifo_path.c_str (), O_WRONLY);

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

    sem_full_key = get_sem_key (options.guild_id);
    sem = create_sem (sem_full_key);

    // create a child
    p_info.cpid = fork ();
    if (p_info.cpid == -1)
        {
            perror ("fork");
            init_error = ERR_SFORK;

            clear_sem (sem, sem_full_key);

            goto err_sfork;
        }

    if (p_info.cpid == 0)
        {
            // close unrelated fds
            handle_worker_fork ();

            run_standalone (options, p_info, sem);
        }

    close (cwritefd); /* Close unused write end */
    close (creadfd);  /* Close unused read end */

    cwritefd = -1;
    creadfd = -1;

    do_sem_wait (sem, sem_full_key);
    sem = SEM_FAILED;

    // prepare required data for polling
    prfds[0].events = POLLIN;
    pwfds[0].events = POLLOUT;

    prfds[0].fd = preadfd;
    pwfds[0].fd = pwritefd;

    // main loop, breaking means exiting
    while (!options.panic_break)
        {
            // poll ffmpeg stdout
            int read_has_event = poll (prfds, 1, 10);
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
                                                      &input_read_size)
                                        == -1)
                                        {
                                            options.panic_break = true;
                                            break;
                                        }

                                    if (read_command (options) == 0)
                                        break;
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
                            if (write_stdout (out_buffer, &input_read_size)
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

            read_command (options);
            if (options.panic_break)
                break;

            // recreate ffmpeg process to update filter chain
            if (!options.seek_to.empty ())
                {
                    close (pwritefd);

                    // signal ffmpeg to stop keep reading input file
                    kill (p_info.cpid, SIGTERM);

                    // discard all buffer since we are seeking,
                    // any old buffer are irrelevant
                    while (read (preadfd, rest_buffer, BUFFER_SIZE) > 0)
                        ;

                    // wait for child to finish transferring data
                    waitpid (p_info.cpid, &cstatus, 0);
                    if (options.debug)
                        fprintf (stderr, "processor child status: %d\n",
                                 cstatus);

                    // close read fd
                    close (preadfd);

                    preadfd = -1;

                    cstatus = 0;

                    // do the same setup routine as startup
                    if (pipe (p_info.ppipefd) == -1)
                        {
                            perror ("ppipe");
                            error_status = ERR_LPIPE;

                            notify_seek_done ();

                            break;
                        }
                    preadfd = p_info.ppipefd[0];
                    cwritefd = p_info.ppipefd[1];

                    if (pipe (p_info.cpipefd) == -1)
                        {
                            perror ("cpipe");
                            init_error = ERR_SPIPE;

                            notify_seek_done ();

                            close (preadfd);
                            close (cwritefd);

                            preadfd = -1;
                            cwritefd = -1;
                            break;
                        }
                    creadfd = p_info.cpipefd[0];
                    pwritefd = p_info.cpipefd[1];

                    sem_full_key = get_sem_key (options.guild_id);
                    sem = create_sem (sem_full_key);

                    p_info.cpid = fork ();
                    if (p_info.cpid == -1)
                        {
                            perror ("fork");
                            error_status = ERR_LFORK;

                            notify_seek_done ();

                            close (preadfd);
                            close (cwritefd);
                            close (pwritefd);
                            close (creadfd);

                            preadfd = -1;
                            cwritefd = -1;
                            pwritefd = -1;
                            creadfd = -1;

                            clear_sem (sem, sem_full_key);

                            break;
                        }

                    if (p_info.cpid == 0)
                        {
                            // close unrelated fds to avoid deadlock
                            handle_worker_fork ();

                            run_standalone (options, p_info, sem);
                        }

                    close (cwritefd); /* Close unused write end */
                    close (creadfd);  /* Close unused write end */

                    cwritefd = -1;
                    creadfd = -1;

                    do_sem_wait (sem, sem_full_key);
                    sem = SEM_FAILED;

                    // update fd to poll
                    prfds[0].fd = preadfd;
                    pwfds[0].fd = pwritefd;

                    // mark changes done
                    options.seek_to = "";

                    // clear effect buffer and let it respawn by
                    // manage_processor call below
                    helper_processor::shutdown_chain (true);

                    // notify streaming thread
                    notify_seek_done ();
                }

            helper_processor::manage_processor (options, handle_helper_fork);

            // !TODO: put volume to always in the last chain
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
    // fds to close: preadfd pwritefd write_fifo
    close_valid_fd (&pwritefd);

    // signal ffmpeg to stop keep reading input file
    kill (p_info.cpid, SIGTERM);

    // read the rest of data before closing ffmpeg stdout
    // as it was signaled to terminate, we can read until its stdout is done
    // transferring data and finally closed
    while ((last_read_size = read (preadfd, rest_buffer, BUFFER_SIZE)) > 0)
        {
            if (write_stdout (rest_buffer, &last_read_size) == -1)
                {
                    options.panic_break = true;
                    break;
                };
        }

    helper_processor::shutdown_chain ();

    // wait for childs to make sure they're dead and
    // prevent them to become zombies
    if (options.debug)
        fprintf (stderr, "waiting for child\n");

    cstatus = 0;

    waitpid (p_info.cpid, &cstatus, 0); /* Wait for child */
    if (options.debug)
        fprintf (stderr, "processor child status: %d\n", cstatus);

    close_valid_fd (&preadfd);
    close_valid_fd (&write_fifo);

    if (options.debug)
        fprintf (stderr, "fds closed\n");

    close (STDOUT_FILENO);
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
    // close (process_options.child_write_fd);
    // process_options.child_write_fd = -1;

    close (STDOUT_FILENO);
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

} // musicat::audio_processing
