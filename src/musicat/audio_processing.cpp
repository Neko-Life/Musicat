#include "musicat/audio_processing.h"
#include "musicat/audio_config.h"
#include "musicat/child.h"
#include "musicat/child/command.h"
#include "musicat/config.h"
#include "musicat/helper_processor.h"
#include "musicat/mctrack.h"
#include "musicat/musicat.h"
#include "opus/opus.h"
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <string>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace musicat::audio_processing
{

// need to make this global to close it on worker fork
// processor audio stream out
int stdin_fifo, fifo_status, stdout_fifo;

int write_fifo = -1,
    // standalone ffmpeg stdout read end
    preadfd = -1,
    // standalone ffmpeg runtime command/stdin write end
    pwritefd = -1,
    // standalone ffmpeg stdout write end
    cwritefd = -1,
    // standalone ffmpeg stdin read end
    creadfd = -1,
    // standalone ffmpeg exit status/signal
    cstatus = 0;

// whether current instance have notified parent that it's ready
// through its stdout
bool notified = false;

inline constexpr const char audio_cmd_str[]
    = "[audio_processing::read_command ";
inline constexpr const size_t audio_cmd_str_size
    = (sizeof (audio_cmd_str) / sizeof (*audio_cmd_str));

inline constexpr const char *audio_cmd_str2 = "%s] Processor command: %s\n";

// if this error then exit by returning
int
init_fifos (const child::command::command_options_t &process_options)
{
    run_processor_error_t init_error = SUCCESS;

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
    close_valid_fd (&stdin_fifo);
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
    close_valid_fd (&stdout_fifo);
    if (fifo_status == -1)
        {
            perror ("dup2 stdout_fifo");
            init_error = ERR_SFIFO;
            goto err_sfifo5;
        }

    return init_error;

err_sfifo5:
err_sfifo4:
err_sfifo3:
err_sfifo2:
    close_valid_fd (&write_fifo);
err_sfifo1:

    close (STDOUT_FILENO);
    return init_error;
}

static int
run_standalone (const processor_options_t &options,
                const processor_states_t &p_info, sem_t *sem)
{
    // request kernel to kill little self when parent dies
    if (prctl (PR_SET_PDEATHSIG, SIGTERM) == -1)
        {
            perror ("child standalone prctl");
            child::do_sem_post (sem);
            _exit (EXIT_FAILURE);
        }

    preadfd = p_info.ppipefd[0];
    int cwritefd = p_info.ppipefd[1];
    int creadfd = p_info.cpipefd[0];
    pwritefd = p_info.cpipefd[1];

    close_valid_fd (&preadfd);  /* Close unused read end */
    close_valid_fd (&pwritefd); /* Close unused read end */

    int status;
    status = dup2 (cwritefd, STDOUT_FILENO);
    close_valid_fd (&cwritefd);
    if (status == -1)
        {
            perror ("child standalone dout");
            child::do_sem_post (sem);
            _exit (EXIT_FAILURE);
        }

    status = dup2 (creadfd, STDIN_FILENO);
    close_valid_fd (&creadfd);
    if (status == -1)
        {
            perror ("child standalone din");
            child::do_sem_post (sem);
            _exit (EXIT_FAILURE);
        }

    bool debug = get_debug_state ();

    // redirect ffmpeg stderr to /dev/null
    if (!debug)
        {
            // redirect ffmpeg stderr to /dev/null
            int dnull = open ("/dev/null", O_WRONLY);
            dup2 (dnull, STDERR_FILENO);
            close_valid_fd (&dnull);
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

    const std::string vol_arg
        = "volume=" + std::to_string ((float)options.volume / (float)100);

#ifdef AUDIO_INPUT_USE_EXCITER
    const std::string fargs = "aexciter," + vol_arg;
#endif

    char *rest_args[] = { "-v",
                          "debug",
#ifdef FFMPEG_REALTIME
                          "-probesize",
                          "32",
#endif
                          "-i",
                          (char *)file_path.c_str (),
                          "-af",
#ifdef AUDIO_INPUT_USE_EXCITER
                          (char *)fargs.c_str (),
#else
                          (char *)vol_arg.c_str (),
#endif
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

    if (debug)
        for (unsigned long i = 0; i < (sizeof (args) / sizeof (args[0])); i++)
            {
                if (!args[i])
                    break;

                fprintf (stderr, "%s\n", args[i]);
            }

    child::do_sem_post (sem);
    execvp ("ffmpeg", args);

    perror ("child standalone exit");
    _exit (EXIT_FAILURE);
}

int
init_standalone (processor_states_t &p_info,
                 const processor_options_t &options)
{
    run_processor_error_t init_error = SUCCESS;

    const std::string fdbg = "run_standalone:" + options.id;

    sem_t *sem;
    std::string sem_full_key;

    if (pipe (p_info.ppipefd) == -1)
        {
            perror ("ppipe");
            init_error = ERR_SPIPE;
            goto err;
        }
    preadfd = p_info.ppipefd[0];
    cwritefd = p_info.ppipefd[1];

    if (pipe (p_info.cpipefd) == -1)
        {
            perror ("cpipe");
            init_error = ERR_SPIPE;
            goto err;
        }
    creadfd = p_info.cpipefd[0];
    pwritefd = p_info.cpipefd[1];

    sem_full_key = child::get_sem_key (options.id);
    sem = child::create_sem (sem_full_key);

    // create a child
    p_info.cpid = child::worker::call_fork (fdbg.c_str ());
    if (p_info.cpid == -1)
        {
            perror ("fork");
            init_error = ERR_SFORK;

            child::clear_sem (sem, sem_full_key);

            goto err;
        }

    if (p_info.cpid == 0)
        {
            // close unrelated fds
            child::worker::handle_worker_fork ();

            run_standalone (options, p_info, sem);
        }

    close_valid_fd (&cwritefd); /* Close unused write end */
    close_valid_fd (&creadfd);  /* Close unused read end */

    child::do_sem_wait (sem, sem_full_key);
    sem = SEM_FAILED;

    return init_error;

err:
    close_valid_fd (&pwritefd);
    close_valid_fd (&creadfd);
    close_valid_fd (&preadfd);
    close_valid_fd (&cwritefd);

    return init_error;
}

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
            cmd_buf[read_cmd_size] = '\0';

            const bool debug = get_debug_state ();

            if (debug)
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
            ssize_t status
                = helper_processor::run_through_chain (buffer, size);

            if (status == -1)
                return -1;

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

    // poll to check for HUP and ERR
    struct pollfd pwr[1];

    // we dont actually care about waiting for non-blocking write here
    pwr[0].events = POLLOUT;
    pwr[0].fd = write_fifo;

    int has_event = poll (pwr, 1, 0);

    bool hup = has_event != 0
               // check for both HUP and ERR since both means fd is bad
               // and shouldnt be used for writing anymore
               && ((pwr[0].revents & POLLHUP) == POLLHUP
                   || (pwr[0].revents & POLLERR) == POLLERR);

    ssize_t written = 0;
    ssize_t current_written = 0;
    while (!hup
           && (((current_written
                 = write (write_fifo, buffer + written, *size - written))
                > 0)
               || (written < *size)))
        {
            if (current_written < 0)
                {
                    return -1;
                }

            written += current_written;

            has_event = poll (pwr, 1, 0);

            hup = has_event != 0
                  && ((pwr[0].revents & POLLHUP) == POLLHUP
                      || (pwr[0].revents & POLLERR) == POLLERR);
        }

    if (hup)
        // return error when got hup
        return -1;

    *size = 0;

    return written;
}

// this should be called
// inside the streaming thread
int
send_audio_routine (dpp::discord_voice_client *vclient, uint16_t *send_buffer,
                    ssize_t *send_buffer_length, bool no_wait,
                    OpusEncoder *opus_encoder)
{
    // const bool debug = get_debug_state ();
    bool running_state = get_running_state ();

    if (!running_state || !vclient || vclient->terminating)
        {
            return 1;
        }

    const bool debug = get_debug_state ();

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
            if (!opus_encoder)
                return 2;

            /* vclient->send_audio_raw (send_buffer, *send_buffer_length); */
            std::vector<uint16_t> pcmbuf (send_buffer,
                                          send_buffer + *send_buffer_length);

            while (!pcmbuf.empty ())
                {
                    uint8_t packet[MAX_PACKET];

                    const auto pbufsiz = pcmbuf.size ();
                    if (pbufsiz < ENCODE_BUFFER_SIZE)
                        {
                            if (debug)
                                {
                                    fprintf (stderr,
                                             "[audio_processing::send_audio_"
                                             "routine] Found last chunk of "
                                             "PCM buffer with size: %ld\n",
                                             pbufsiz);
                                }

                            pcmbuf.resize (ENCODE_BUFFER_SIZE);
                        }

                    int len = opus_encode (opus_encoder,
                                           (opus_int16 *)pcmbuf.data (),
                                           FRAME_SIZE, packet, MAX_PACKET);

                    if (len < 0 || len > MAX_PACKET)
                        {
                            fprintf (
                                stderr,
                                "[audio_processing::send_audio_routine ERROR] "
                                "opus_encode() returned %d\n",
                                len);

                            return len;
                        }

                    if (len > 2)
                        {
                            vclient->send_audio_opus (packet, len,
                                                      SEND_DURATION);
                        }

                    pcmbuf.erase (pcmbuf.begin (),
                                  pcmbuf.begin () + ENCODE_BUFFER_SIZE);
                }
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

// should be run as a child process
int
run_processor (child::command::command_options_t &process_options)
{
    // since Musicat main process can stop in the middle of stream
    // and thus force closing the output fd (write_fifo) we should
    // ignore SIGPIPE signal to avoid child terminates and to be able to
    // check for HUP
    signal (SIGPIPE, SIG_IGN);

    processor_states_t p_info;

    processor_options_t options = create_options ();
    options.file_path = process_options.file_path;
    /* options.debug = process_options.debug; */
    options.id = process_options.id;
    options.guild_id = process_options.guild_id;
    options.volume = process_options.volume;

    bool debug = get_debug_state ();
    bool has_command = false;

    int error_status = SUCCESS;

    // declare everything here to be able to use goto
    // fds and poll
    struct pollfd prfds[1], pwfds[1];

    // main loop variable definition
    processor_options_t current_options = copy_options (options);
    parse_helper_chain_option (process_options, options);

    helper_processor::manage_processor (options, handle_helper_fork);

    bool write_stdout_err = false;
    int read_has_event = 0;
    bool read_ready = false;
    uint8_t out_buffer[BUFFER_SIZE];
    ssize_t current_read = 0;

    //// fifo
    error_status = init_fifos (process_options);
    if (error_status != SUCCESS)
        goto init_err;

    // prepare required pipes for bidirectional interprocess communication
    //// standalone
    error_status = init_standalone (p_info, options);
    if (error_status != SUCCESS)
        goto init_err;

    // prepare required data for polling
    prfds[0].events = POLLIN;
    pwfds[0].events = POLLOUT;

    prfds[0].fd = preadfd;
    pwfds[0].fd = pwritefd;

    // main loop, breaking means exiting
    while (!options.panic_break)
        {
            // poll ffmpeg stdout
            read_has_event = poll (prfds, 1, 10);
            read_ready = (read_has_event > 0)
                         && (prfds[0].revents & POLLIN) == POLLIN;

            if ((prfds[0].revents & POLLERR) == POLLERR
                || (prfds[0].revents & POLLHUP) == POLLHUP)
                // ffmpeg done reading, lets break
                break;

            while (read_ready
                   && ((current_read = read (preadfd, out_buffer, BUFFER_SIZE))
                       > 0))
                {
                    if (write_stdout (out_buffer, &current_read) == -1)
                        {
                            options.panic_break = true;
                            write_stdout_err = true;
                            break;
                        }

                    if ((has_command = read_command (options) == 0))
                        break;

                    read_has_event = poll (prfds, 1, 0);
                    read_ready = (read_has_event > 0)
                                 && (prfds[0].revents & POLLIN) == POLLIN;
                }

            if (write_stdout_err)
                break;

            // empties the last buffer that usually size less than
            // BUFFER_SIZE
            if (current_read > 0)
                {
                    if (write_stdout (out_buffer, &current_read) == -1)
                        {
                            options.panic_break = true;
                            write_stdout_err = true;
                            break;
                        }
                }

            if (!has_command)
                has_command = read_command (options) == 0;

            if (has_command)
                debug = get_debug_state ();

            has_command = false;

            if (options.panic_break)
                break;

            // recreate ffmpeg process to update filter chain
            if (!options.seek_to.empty ())
                {
                    close_valid_fd (&pwritefd);
                    close_valid_fd (&preadfd);

                    kill (p_info.cpid, SIGTERM);

                    cstatus = child::worker::call_waitpid (p_info.cpid);
                    if (debug)
                        fprintf (stderr, "processor child status: %d\n",
                                 cstatus);

                    cstatus = 0;

                    // do the same setup routine as startup
                    //// standalone
                    error_status = init_standalone (p_info, options);
                    if (error_status != SUCCESS)
                        {
                            notify_seek_done ();
                            goto init_err;
                        }

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

                    // !TODO: poll pwfds?
                    // we can trust ffmpeg to not randomly close its stdin tho
                    // for now

                    write (pwritefd, buf, str_buf.size () + 1);
                    current_options.volume = options.volume;
                }
        }

    // exiting, clean up
    // fds to close: preadfd pwritefd write_fifo
    close_valid_fd (&pwritefd);
    close_valid_fd (&preadfd);

    kill (p_info.cpid, SIGTERM);

    // wait for childs to make sure they're dead and
    // prevent them to become zombies
    if (debug)
        fprintf (stderr, "waiting for child\n");

    cstatus = 0;

    cstatus = child::worker::call_waitpid (p_info.cpid); /* Wait for child */
    if (debug)
        fprintf (stderr, "processor child status: %d\n", cstatus);

    helper_processor::shutdown_chain (write_stdout_err);

    if (debug)
        fprintf (stderr, "fds closed\n");

    return error_status;

init_err:
    helper_processor::shutdown_chain (true);

    return error_status;
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

} // musicat::audio_processing
