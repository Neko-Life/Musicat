/* #include "musicat/audio_processing.h" */
#include "musicat/audio_processing.h"
#include "musicat/child/command.h"
#include "musicat/child/worker.h"
#include "musicat/musicat.h"
#include "musicat/player.h"
#include <oggz/oggz.h>
#include <oggz/oggz_seek.h>
#include <sys/poll.h>
#include <sys/stat.h>
#include <sys/types.h>

// correct frame size with timescale for dpp
#define STREAM_BUFSIZ dpp::send_audio_raw_max_length

#define DPP_AUDIO_BUFFER_LENGTH_SECOND 0.5f
#define SLEEP_ON_BUFFER_THRESHOLD_MS 50

inline constexpr long CHUNK_READ = BUFSIZ * 2;

namespace musicat
{
namespace player
{
using string = std::string;

struct mc_oggz_user_data
{
    dpp::discord_voice_client *voice_client;
    MCTrack &track;
    bool &debug;
};

void
Manager::stream (dpp::discord_voice_client *v, player::MCTrack &track)
{
    const string &fname = track.filename;

    dpp::snowflake server_id = 0;
    std::chrono::_V2::system_clock::time_point start_time;

    const string music_folder_path = get_music_folder_path ();
    const string file_path = music_folder_path + fname;

    if (v && !v->terminating && v->is_ready ())
        {
            bool debug = get_debug_state ();

            server_id = v->server_id;
            if (!server_id)
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
                    throw 2;
                }

            fclose (ofile);
            ofile = NULL;

            track.filesize = ofile_stat.st_size;

            using namespace child::command;
            using child::command::wait_slave_ready;

            std::string server_id_str = std::to_string (server_id);
            std::string slave_id = "processor-" + server_id_str;

            std::string cmd
                = command_options_keys_t.id + '=' + slave_id + ';'
                  + command_options_keys_t.guild_id + '=' + server_id_str + ';'
                  + command_options_keys_t.command + '='
                  + command_execute_commands_t.create_audio_processor + ';'
                  + command_options_keys_t.debug + "=1;"
                  + command_options_keys_t.file_path + '='
                  + sanitize_command_value (file_path);

            std::string exit_cmd = command_options_keys_t.id + '=' + slave_id
                                   + ';' + command_options_keys_t.command + '='
                                   + command_execute_commands_t.shutdown + ';';

            send_command (cmd);

            int status = wait_slave_ready (slave_id, 10);

            if (status == child::worker::ready_status_t.ERR_SLAVE_EXIST)
                {
                    send_command (exit_cmd);
                    // send_command (cmd);
                    // status = wait_slave_ready (slave_id, 10);
                }

            if (status != 0)
                {
                    throw 2;
                }

            const std::string fifo_stream_path
                = audio_processing::get_audio_stream_fifo_path (slave_id);

            int read_fd = open (fifo_stream_path.c_str (), O_RDONLY);

            ofile = fdopen (read_fd, "r");

            // I LOVE C++!!!

            // track.seekable = true;

            // ssize_t read_size = 0;
            // ssize_t last_read_size = 0;
            // ssize_t total_read = 0;
            // uint8_t buffer[STREAM_BUFSIZ];

            bool running_state = get_running_state (), is_stopping;

            // while ((running_state = get_running_state ())
            //        && ((read_size += read (read_fd, buffer + read_size,
            //                                STREAM_BUFSIZ - read_size))
            //            > 0))
            //     {
            //         // if (track.seek_to > -1 && track.seekable)

            //         // prevent infinite loop when done reading the last
            //         // straw of data
            //         const bool is_last_data
            //             = read_size < STREAM_BUFSIZ
            //               && last_read_size == read_size;

            //         if ((is_stopping = is_stream_stopping (server_id))
            //             || is_last_data)
            //             {
            //                 break;
            //             }

            //         last_read_size = read_size;

            //         if (read_size == STREAM_BUFSIZ)
            //             {
            //                 total_read += read_size;

            //                 if ((debug = get_debug_state ()))
            //                     fprintf (stderr,
            //                              "Sending buffer: %ld %ld\n",
            //                              total_read, read_size);

            //                 if (audio_processing::send_audio_routine (
            //                         v, (uint16_t *)buffer, &read_size))
            //                     {
            //                         break;
            //                     }
            //             }
            //     }

            // if (!running_state || is_stopping)
            //     {
            //         v->stop_audio ();
            //     }
            // else if (read_size > 0)
            //     {
            //         // ssize_t padding = STREAM_BUFSIZ - read_size;

            //         // for (; read_size < STREAM_BUFSIZ; read_size++)
            //         //     {
            //         //         buffer[read_size] = 0;
            //         //     }

            //         // if (debug)
            //         //     {
            //         //         fprintf (stderr, "Added padding: %ld\n",
            //         //                  padding);
            //         //     }

            //         if (debug)
            //             fprintf (stderr, "Final buffer: %ld %ld\n",
            //                      (total_read += read_size), read_size);

            //         audio_processing::send_audio_routine (
            //             v, (uint16_t *)buffer, &read_size, true);
            //     }

            // fprintf (stderr, "Exiting %ld\n", server_id);
            // close (read_fd);

            // send_command (exit_cmd);

            //         // !TODO: get options from worker
            //         audio_processing::processor_options_t options
            //             = get_slave_options ();

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
                            //     fprintf (stderr, "OGGZ Read Bytes: %ld\n",
                            //              packet->op.bytes);

                            data->voice_client->send_audio_opus (
                                packet->op.packet, packet->op.bytes);

                            if (!data->track.seekable && packet->op.b_o_s == 0)
                                {
                                    data->track.seekable = true;
                                }

                            return 0;
                        },
                        (void *)&data);

                    // stream loop
                    while ((running_state = get_running_state ()) && v
                           && !v->terminating)
                        {
                            if ((is_stopping = is_stream_stopping (server_id)))
                                break;

                            debug = get_debug_state ();

                            // if (track.seek_to > -1 && track.seekable)
                            //     {
                            //         oggz_off_t offset = oggz_seek (
                            //             track_og, track.seek_to, SEEK_SET);

                            //         if (debug)
                            //             fprintf (stderr,
                            //                      "[Manager::stream] "
                            //                      "Seeking from: "
                            //                      "%ld\nTarget: %ld\n"
                            //                      "Result offset: %ld\n",
                            //                      track.current_byte,
                            //                      track.seek_to, offset);

                            //         track.current_byte = offset;
                            //         track.seek_to = -1;
                            //     }

                            const long read_bytes
                                = oggz_read (track_og, CHUNK_READ);

                            track.current_byte += read_bytes;

                            if (debug)
                                printf ("[Manager::stream] "
                                        "[guild_id] [size] "
                                        "[chunk] [read_bytes]: "
                                        "%ld %ld %ld %ld\n",
                                        server_id, track.filesize, read_bytes,
                                        track.current_byte);

                            while ((running_state = get_running_state ()) && v
                                   && !v->terminating
                                   && v->get_secs_remaining ()
                                          > DPP_AUDIO_BUFFER_LENGTH_SECOND)
                                {
                                    std::this_thread::sleep_for (
                                        std::chrono::milliseconds (
                                            SLEEP_ON_BUFFER_THRESHOLD_MS));
                                }

                            // eof
                            if (!read_bytes)
                                break;
                        }
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

            if (debug)
                fprintf (stderr, "Exiting %ld\n", server_id);

            send_command (exit_cmd);

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
