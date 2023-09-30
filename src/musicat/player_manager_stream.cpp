#include "musicat/audio_processing.h"
#include "musicat/child.h"
#include "musicat/child/command.h"
#include "musicat/child/worker.h"
#include "musicat/config.h"
#include "musicat/musicat.h"
#include "musicat/player.h"
#include <memory>
#include <oggz/oggz.h>
#include <oggz/oggz_seek.h>
#include <sys/poll.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <thread>

#ifdef MUSICAT_USE_PCM

#define STREAM_BUFSIZ STREAM_BUFSIZ_PCM
#define CHUNK_READ CHUNK_READ_PCM
#define DRAIN_CHUNK DRAIN_CHUNK_PCM

#else

#define STREAM_BUFSIZ STREAM_BUFSIZ_OPUS
#define CHUNK_READ CHUNK_READ_OPUS
#define DRAIN_CHUNK DRAIN_CHUNK_OPUS

#endif

// correct frame size with timescale for dpp
#define STREAM_BUFSIZ_PCM dpp::send_audio_raw_max_length
inline constexpr long CHUNK_READ_PCM = BUFSIZ * 2;
inline constexpr long DRAIN_CHUNK_PCM = BUFSIZ / 2;

inline constexpr long STREAM_BUFSIZ_OPUS = BUFSIZ / 8;
inline constexpr long CHUNK_READ_OPUS = BUFSIZ / 2;
inline constexpr long DRAIN_CHUNK_OPUS = BUFSIZ / 4;

namespace musicat
{
namespace player
{
using string = std::string;
namespace cc = child::command;

struct mc_oggz_user_data
{
    dpp::discord_voice_client *voice_client;
    MCTrack &track;
    bool &debug;
};

struct handle_effect_chain_change_states_t
{
    std::shared_ptr<Player> &guild_player;
    player::MCTrack &track;
    int &command_fd;
    int &read_fd;
    OGGZ *track_og;
};

struct run_stream_loop_states_t
{
    dpp::discord_voice_client *&v;
    player::MCTrack &track;
    dpp::snowflake &server_id;
    OGGZ *&track_og;
    bool &running_state;
    bool &is_stopping;
    bool &debug;
};

void
handle_effect_chain_change (handle_effect_chain_change_states_t &states)
{
    // do not send drain buffer
    bool no_send = false;

    const bool track_seek_queried = !states.track.seek_to.empty ();

    if (track_seek_queried)
        {
            const std::string cmd = cc::command_options_keys_t.command + '='
                                    + cc::command_options_keys_t.seek + ';'
                                    + cc::command_options_keys_t.seek + '='
                                    + states.track.seek_to + ';';

            cc::write_command (cmd, states.command_fd, "Manager::stream");

            states.track.seek_to = "";

            // struct pollfd nfds[1];
            // nfds[0].events = POLLIN;
            // nfds[0].fd = read_fd;

            // int n_event = poll (nfds, 1, 100);
            // bool n_ready
            //     = (n_event > 0)
            //       && (nfds[0].revents & POLLIN);

            // if (n_ready)
            //     {
            //         char read_buf[CMD_BUFSIZE];
            //         read (notification_fd, read_buf,
            //               CMD_BUFSIZE);

            //         if (read_buf[0] != '0')
            //             {
            //                 // !TODO: change to
            //                 other
            //                 // number and add
            //                 handler
            //                 // for it
            //                 throw_error = 2;
            //                 break;
            //             }
            //     }

            no_send = true;
        }

    const bool volume_queried = states.guild_player->set_volume != -1;

    if (volume_queried)
        {
            const std::string cmd
                = cc::command_options_keys_t.command + '='
                  + cc::command_options_keys_t.volume + ';'
                  + cc::command_options_keys_t.volume + '='
                  + std::to_string (states.guild_player->set_volume) + ';';

            cc::write_command (cmd, states.command_fd, "Manager::stream");

            states.guild_player->volume = states.guild_player->set_volume;
            states.guild_player->set_volume = -1;
        }

    const bool equalizer_queried
        = !states.guild_player->set_equalizer.empty ();

    if (equalizer_queried)
        {
            const std::string new_equalizer
                = states.guild_player->set_equalizer == "0"
                      ? ""
                      : states.guild_player->set_equalizer;

            const std::string cmd
                = cc::command_options_keys_t.command + '='
                  + cc::command_options_keys_t.helper_chain + ';'
                  + cc::command_options_keys_t.helper_chain + '='
                  + cc::sanitize_command_value (new_equalizer) + ';';

            cc::write_command (cmd, states.command_fd, "Manager::stream");

            states.guild_player->equalizer = new_equalizer;
            states.guild_player->set_equalizer = "";
        }

    const bool queried_cmd
        = track_seek_queried || volume_queried || equalizer_queried;

    if (!queried_cmd)
        {
            return;
        }

    //////////////////////////////////////////////////

    if (no_send)
        {
            //////////////////////////////////////////////////
            ssize_t drain_size = 0;
            bool less_buffer_encountered = false;

            // drain current output
            struct pollfd pfds[1];
            pfds[0].events = POLLIN;
            pfds[0].fd = states.read_fd;

            int has_event = poll (pfds, 1, 1000);
            bool drain_ready = (has_event > 0) && (pfds[0].revents & POLLIN);

            //////////////////////////////////////////////////

            // have some patient and wait for 1000 ms each poll
            // cuz if the pipe isn't really empty we will not have a smooth
            // seek
            char drain_buf[DRAIN_CHUNK];
            while (drain_ready
                   && ((drain_size
                        = read (states.read_fd, drain_buf, DRAIN_CHUNK))
                       > 0))
                {
                    if (drain_size < DRAIN_CHUNK)
                        {
                            // might be the last buffer, might be not
                            if (less_buffer_encountered)
                                {
                                    // this is the second time we encountered
                                    // buffer with size less than chunk read,
                                    // lets break
                                    break;
                                }

                            // lets set a flag for next encounter to break
                            less_buffer_encountered = true;
                        }

                    has_event = poll (pfds, 1, 1000);
                    drain_ready
                        = (has_event > 0) && (pfds[0].revents & POLLIN);
                }

            return;
        }

#ifndef MUSICAT_USE_PCM
        // read buffer through liboggz once
        // int r_count = 0;
        // while (drain_ready && (r_count < 2)
        //        && ((drain_size = oggz_read (states.track_og, CHUNK_READ)) >
        //        0))
        //     {
        //         r_count++;
        //         if (drain_size < CHUNK_READ)
        //             {
        //                 // might be the last buffer, might be not
        //                 if (less_buffer_encountered)
        //                     {
        //                         // this is the second time we encountered
        //                         // buffer with size less than chunk read,
        //                         // lets break
        //                         break;
        //                     }

        //                 // lets set a flag for next encounter to break
        //                 less_buffer_encountered = true;
        //             }

        //         has_event = poll (pfds, 1, 0);
        //         drain_ready = (has_event > 0) && (pfds[0].revents & POLLIN);
        //     }
#else
        // read and send pcm
#endif
}

void
run_stream_loop (Manager *manager, run_stream_loop_states_t &states,
                 handle_effect_chain_change_states_t &effect_states)
{
    while ((states.running_state = get_running_state ()) && states.v
           && !states.v->terminating)
        {
            if ((states.is_stopping
                 = manager->is_stream_stopping (states.server_id)))
                break;

            states.debug = get_debug_state ();

            handle_effect_chain_change (effect_states);

            const long read_bytes = oggz_read (states.track_og, CHUNK_READ);

            states.track.current_byte += read_bytes;

            if (states.debug)
                std::cerr << "[Manager::stream] "
                             "[guild_id] [size] "
                             "[chunk] [read_bytes]: "
                          << states.server_id << ' ' << states.track.filesize
                          << ' ' << read_bytes << '\n';

            while ((states.running_state = get_running_state ()) && states.v
                   && !states.v->terminating
                   && states.v->get_secs_remaining ()
                          > DPP_AUDIO_BUFFER_LENGTH_SECOND)
                {
                    std::this_thread::sleep_for (std::chrono::milliseconds (
                        SLEEP_ON_BUFFER_THRESHOLD_MS));
                }

            // eof
            if (!read_bytes)
                break;
        }
}

void
Manager::stream (dpp::discord_voice_client *v, player::MCTrack &track)
{
    const string &fname = track.filename;

    dpp::snowflake server_id = 0;
    std::chrono::high_resolution_clock::time_point start_time;

    const string music_folder_path = get_music_folder_path ();
    const string file_path = music_folder_path + fname;

    if (v && !v->terminating && v->is_ready ())
        {
            bool debug = get_debug_state ();

            server_id = v->server_id;
            auto player_manager = get_player_manager_ptr ();
            auto guild_player = player_manager
                                    ? player_manager->get_player (server_id)
                                    : nullptr;

            if (!server_id || !guild_player)
                throw 2;

            FILE *ofile = fopen (file_path.c_str (), "r");

            if (!ofile)
                {
                    std::filesystem::create_directory (music_folder_path);
                    throw 2;
                }

            struct stat ofile_stat;
            if (fstat (fileno (ofile), &ofile_stat) != 0)
                {
                    fclose (ofile);
                    ofile = NULL;
                    throw 2;
                }

            fclose (ofile);
            ofile = NULL;

            track.filesize = ofile_stat.st_size;

            std::string server_id_str = std::to_string (server_id);
            std::string slave_id = "processor-" + server_id_str;

            std::string cmd
                = cc::command_options_keys_t.id + '=' + slave_id + ';'
                  + cc::command_options_keys_t.guild_id + '=' + server_id_str

                  + ';' + cc::command_options_keys_t.command + '='
                  + cc::command_execute_commands_t.create_audio_processor
                  + ';';

            if (debug)
                {
                    cmd += cc::command_options_keys_t.debug + "=1;";
                }

            cmd += cc::command_options_keys_t.file_path + '='
                   + cc::sanitize_command_value (file_path) + ';'

                   + cc::command_options_keys_t.volume + '='
                   + std::to_string (guild_player->volume) + ';';

            if (!guild_player->equalizer.empty ())
                cmd += cc::command_options_keys_t.helper_chain + '='
                       + cc::sanitize_command_value (guild_player->equalizer)
                       + ';';

            // !TODO: convert current byte to timestamp string
            // + cc::command_options_keys_t.seek + '=' +
            // track.current_byte
            // + ';';

            const std::string exit_cmd
                = cc::command_options_keys_t.id + '=' + slave_id + ';'
                  + cc::command_options_keys_t.command + '='
                  + cc::command_execute_commands_t.shutdown + ';';

            cc::send_command (cmd);

            int status = cc::wait_slave_ready (slave_id, 10);

            if (status == child::worker::ready_status_t.ERR_SLAVE_EXIST)
                {
                    cc::send_command (exit_cmd);
                    // send_command (cmd);
                    // status = wait_slave_ready (slave_id, 10);
                }

            if (status != 0)
                {
                    throw 2;
                }

            const std::string fifo_stream_path
                = audio_processing::get_audio_stream_fifo_path (slave_id);

            const std::string fifo_command_path
                = audio_processing::get_audio_stream_stdin_path (slave_id);

            const std::string fifo_notify_path
                = audio_processing::get_audio_stream_stdout_path (slave_id);

            // OPEN FIFOS
            int read_fd = open (fifo_stream_path.c_str (), O_RDONLY);
            if (read_fd < 0)
                {
                    throw 2;
                }

            int command_fd = open (fifo_command_path.c_str (), O_WRONLY);
            if (command_fd < 0)
                {
                    close (read_fd);
                    throw 2;
                }

            int notification_fd = open (fifo_notify_path.c_str (), O_RDONLY);
            if (notification_fd < 0)
                {
                    close (read_fd);
                    close (command_fd);
                    throw 2;
                }

            handle_effect_chain_change_states_t effect_states
                = { guild_player, track, command_fd, read_fd, NULL };

            int throw_error = 0;
            bool running_state = get_running_state (), is_stopping;

            // I LOVE C++!!!

            // track.seekable = true;

            // using raw pcm need to change ffmpeg output format to s16le!
#ifdef MUSICAT_USE_PCM
            ssize_t read_size = 0;
            ssize_t last_read_size = 0;
            ssize_t total_read = 0;
            uint8_t buffer[STREAM_BUFSIZ];

            while ((running_state = get_running_state ())
                   && ((read_size += read (read_fd, buffer + read_size,
                                           STREAM_BUFSIZ - read_size))
                       > 0))
                {
                    // if (track.seek_to > -1 && track.seekable)

                    // prevent infinite loop when done reading the last
                    // straw of data
                    const bool is_last_data
                        = read_size < (ssize_t)STREAM_BUFSIZ
                          && last_read_size == read_size;

                    if ((is_stopping = is_stream_stopping (server_id))
                        || is_last_data)
                        {
                            break;
                        }

                    handle_effect_chain_change (effect_states);

                    last_read_size = read_size;

                    // !TODO: calculate pcm duration based on size
                    if (read_size == STREAM_BUFSIZ)
                        {
                            total_read += read_size;

                            if ((debug = get_debug_state ()))
                                fprintf (stderr, "Sending buffer: %ld %ld\n",
                                         total_read, read_size);

                            if (audio_processing::send_audio_routine (
                                    v, (uint16_t *)buffer, &read_size))
                                {
                                    break;
                                }

                            float outbuf_duration;

                            while ((running_state = get_running_state ()) && v
                                   && !v->terminating
                                   && ((outbuf_duration
                                        = v->get_secs_remaining ())
                                       > DPP_AUDIO_BUFFER_LENGTH_SECOND))
                                {
                                    // isn't very pretty for the terminal,
                                    // disable for now if ((debug =
                                    // get_debug_state ()))
                                    //     {
                                    //         static std::chrono::time_point
                                    //             start_time
                                    //             = std::chrono::
                                    //                 high_resolution_clock::
                                    //                     now ();

                                    //         auto end_time = std::chrono::
                                    //             high_resolution_clock::now
                                    //             ();

                                    //         auto done
                                    //             =
                                    //             std::chrono::duration_cast<
                                    //                 std::chrono::
                                    //                     milliseconds> (
                                    //                 end_time - start_time);

                                    //         start_time = end_time;

                                    //         fprintf (
                                    //             stderr,
                                    //             "[audio_processing::send_"
                                    //             "audio_routine] "
                                    //             "outbuf_duration: %f >
                                    //             %f\n", outbuf_duration,
                                    //             DPP_AUDIO_BUFFER_LENGTH_SECOND);

                                    //         fprintf (stderr,
                                    //                  "[audio_processing::send_"
                                    //                  "audio_routine] Delay "
                                    //                  "between send: %ld "
                                    //                  "milliseconds\n",
                                    //                  done.count ());
                                    //     }

                                    handle_effect_chain_change (effect_states);

                                    std::this_thread::sleep_for (
                                        std::chrono::milliseconds (
                                            SLEEP_ON_BUFFER_THRESHOLD_MS));
                                }
                        }
                }

            if ((read_size > 0) && running_state && !is_stopping)
                {
                    // ssize_t padding = STREAM_BUFSIZ - read_size;

                    // for (; read_size < STREAM_BUFSIZ; read_size++)
                    //     {
                    //         buffer[read_size] = 0;
                    //     }

                    // if (debug)
                    //     {
                    //         fprintf (stderr, "Added padding: %ld\n",
                    //                  padding);
                    //     }

                    if (debug)
                        fprintf (stderr, "Final buffer: %ld %ld\n",
                                 (total_read += read_size), read_size);

                    audio_processing::send_audio_routine (
                        v, (uint16_t *)buffer, &read_size, true);
                }

            close (read_fd);

            // using raw pcm code ends here
#else
            // using OGGZ need to change ffmpeg output format to opus!
            ofile = fdopen (read_fd, "r");

            if (!ofile)
                {
                    close (read_fd);
                    close (command_fd);
                    close (notification_fd);
                    throw 2;
                }

            OGGZ *track_og = oggz_open_stdio (ofile, OGGZ_READ);

            if (track_og)
                {
                    mc_oggz_user_data data = { v, track, debug };

                    oggz_set_read_callback (
                        track_og, -1,
                        [] (OGGZ *oggz, oggz_packet *packet, long serialno,
                            void *user_data) {
                            mc_oggz_user_data *data
                                = (mc_oggz_user_data *)user_data;

                            // if (data->debug)
                            //     fprintf (stderr, "OGGZ Read Bytes: %ld\n ",
                            //              packet->op.bytes);

                            data->voice_client->send_audio_opus (
                                packet->op.packet, packet->op.bytes);

                            // if (!data->track.seekable && packet->op.b_o_s ==
                            // 0)
                            //     {
                            //         data->track.seekable = true;
                            //     }

                            return 0;
                        },
                        (void *)&data);

                    struct run_stream_loop_states_t states = {
                        v,           track, server_id, track_og, running_state,
                        is_stopping, debug,
                    };

                    effect_states.track_og = track_og;

                    // stream loop
                    run_stream_loop (this, states, effect_states);
                }
            else
                {
                    fprintf (stderr,
                             "[Manager::stream ERROR] Can't open file for "
                             "reading: %ld '%s'\n",
                             server_id, file_path.c_str ());
                }

            // read_fd already closed along with this
            oggz_close (track_og);
            track_og = NULL;

            // using OGGZ code ends here
#endif

            close (command_fd);
            command_fd = -1;
            close (notification_fd);
            notification_fd = -1;

            if (debug)
                std::cerr << "Exiting " << server_id << '\n';

            cc::send_command (exit_cmd);

            if (!running_state || is_stopping)
                {
                    // clear voice client buffer
                    v->stop_audio ();
                }

            auto end_time = std::chrono::high_resolution_clock::now ();
            auto done = std::chrono::duration_cast<std::chrono::milliseconds> (
                end_time - start_time);

            if (debug)
                {
                    fprintf (stderr, "Done streaming for %ld milliseconds\n",
                             done.count ());
                    // fprintf (stderr, "audio_processing status: %d\n",
                    // status);
                }

            if (throw_error != 0)
                {
                    throw throw_error;
                }

            // if (status != audio_processing::SUCCESS)
            //     throw 2;
        }
    else
        throw 1;
}

bool
Manager::is_stream_stopping (const dpp::snowflake &guild_id)
{
    std::lock_guard<std::mutex> lk (sq_m);

    if (guild_id && stop_queue[guild_id])
        return true;

    return false;
}

int
Manager::set_stream_stopping (const dpp::snowflake &guild_id)
{
    std::lock_guard<std::mutex> lk (sq_m);

    if (guild_id)
        {
            stop_queue[guild_id] = true;
            return 0;
        }

    return 1;
}

int
Manager::clear_stream_stopping (const dpp::snowflake &guild_id)
{
    if (guild_id)
        {
            std::lock_guard<std::mutex> lk (sq_m);
            stop_queue.erase (guild_id);

            return 0;
        }

    return 1;
}

void
Manager::set_processor_state (std::string &server_id_str,
                              processor_state_t state)
{
    std::lock_guard<std::mutex> lk (this->as_m);

    this->processor_states[server_id_str] = state;
}

processor_state_t
Manager::get_processor_state (std::string &server_id_str)
{
    std::lock_guard<std::mutex> lk (this->as_m);
    auto i = this->processor_states.find (server_id_str);

    if (i == this->processor_states.end ())
        {
            return PROCESSOR_NULL;
        }

    return i->second;
}

bool
Manager::is_processor_ready (std::string &server_id_str)
{
    return get_processor_state (server_id_str) & PROCESSOR_READY;
}

bool
Manager::is_processor_dead (std::string &server_id_str)
{
    return get_processor_state (server_id_str) & PROCESSOR_DEAD;
}

} // player
} // musicat
