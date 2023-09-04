/* #include "musicat/audio_processing.h" */
#include "musicat/audio_processing.h"
#include "musicat/child/command.h"
#include "musicat/musicat.h"
#include "musicat/player.h"
#include <oggz/oggz.h>
#include <oggz/oggz_seek.h>
#include <sys/poll.h>
#include <sys/stat.h>

// correct frame size with timescale for dpp
#define STREAM_BUFSIZ 11520

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

            const size_t fsize = ofile_stat.st_size;

            track.filesize = fsize;

            // !TODO: remove this testing
            if (server_id != 0)
                {
                    using namespace child::command;
                    using child::command::wait_slave_ready;

                    std::string server_id_str = std::to_string (server_id);
                    std::string slave_id = "processor-" + server_id_str;

                    std::string cmd
                        = command_options_keys_t.id + '=' + slave_id + ';'
                          + command_options_keys_t.guild_id + '='
                          + server_id_str + ';'
                          + command_options_keys_t.command + '='
                          + command_execute_commands_t.create_audio_processor
                          + ';' + command_options_keys_t.debug + "=1;"
                          + command_options_keys_t.file_path + '='
                          + sanitize_command_value (file_path);

                    send_command (cmd);

                    int status = wait_slave_ready (slave_id, 10);

                    if (status != 0)
                        {
                            throw 2;
                        }

                    const std::string fifo_stream_path
                        = audio_processing::get_audio_stream_fifo_path (
                            slave_id);

                    int read_fd = open (fifo_stream_path.c_str (), O_RDONLY);

                    ssize_t read_size = 0;
                    ssize_t last_read_size = 0;
                    ssize_t total_read = 0;
                    uint8_t buffer[STREAM_BUFSIZ];

                    bool running_state;
                    while ((running_state = get_running_state ())
                           && ((read_size += read (read_fd, buffer + read_size,
                                                   STREAM_BUFSIZ - read_size))
                               > 0))
                        {
                            // prevent infinite loop when done reading the last
                            // straw of data
                            if (last_read_size == read_size
                                && read_size < STREAM_BUFSIZ)
                                {
                                    break;
                                }

                            last_read_size = read_size;

                            if (read_size == STREAM_BUFSIZ)
                                {
                                    total_read += read_size;

                                    if ((debug = get_debug_state ()))
                                        fprintf (stderr,
                                                 "Sending buffer: %ld %ld\n",
                                                 total_read, read_size);

                                    if (audio_processing::send_audio_routine (
                                            v, (uint16_t *)buffer, &read_size))
                                        {
                                            break;
                                        }
                                }
                        }

                    if (debug)
                        fprintf (stderr, "Final buffer: %ld %ld\n",
                                 total_read + read_size, read_size);

                    audio_processing::send_audio_routine (
                        v, (uint16_t *)buffer, &read_size, true);

                    if (!running_state)
                        {
                            v->stop_audio ();
                        }

                    fprintf (stderr, "Exiting %ld\n", server_id);
                    close (read_fd);

                    std::string exit_cmd
                        = command_options_keys_t.id + '=' + slave_id + ';'
                          + command_options_keys_t.command + '='
                          + command_execute_commands_t.shutdown + ';';

                    send_command (exit_cmd);

                    //         // !TODO: get options from worker
                    //         audio_processing::processor_options_t options
                    //             = get_slave_options ();
                }

            // OGGZ *track_og = oggz_open_stdio (ofile, OGGZ_READ);

            // if (track_og)
            //     {
            //         mc_oggz_user_data data = { v, track, debug };

            //         oggz_set_read_callback (
            //             track_og, -1,
            //             [] (OGGZ *oggz, oggz_packet *packet, long serialno,
            //                 void *user_data) {
            //                 mc_oggz_user_data *data
            //                     = (mc_oggz_user_data *)user_data;

            //                 data->voice_client->send_audio_opus (
            //                     packet->op.packet, packet->op.bytes);

            //                 if (!data->track.seekable && packet->op.b_o_s ==
            //                 0)
            //                     {
            //                         data->track.seekable = true;
            //                     }

            //                 return 0;
            //             },
            //             (void *)&data);

            //         // stream loop
            //         while (v && !v->terminating)
            //             {
            //                 if (is_stream_stopping (server_id))
            //                     break;

            //                 debug = get_debug_state ();

            //                 static const constexpr long CHUNK_READ
            //                     = BUFSIZ * 2;

            //                 if (track.seek_to > -1 && track.seekable)
            //                     {
            //                         oggz_off_t offset = oggz_seek (
            //                             track_og, track.seek_to, SEEK_SET);

            //                         if (debug)
            //                             fprintf (stderr,
            //                                      "[Manager::stream] "
            //                                      "Seeking from: "
            //                                      "%ld\nTarget: %ld\n"
            //                                      "Result offset: %ld\n",
            //                                      track.current_byte,
            //                                      track.seek_to, offset);

            //                         track.current_byte = offset;
            //                         track.seek_to = -1;
            //                     }

            //                 const long read_bytes
            //                     = oggz_read (track_og, CHUNK_READ);

            //                 track.current_byte += read_bytes;

            //                 if (debug)
            //                     printf ("[Manager::stream] "
            //                             "[guild_id] [size] "
            //                             "[chunk] [read_bytes]: "
            //                             "%ld %ld %ld %ld\n",
            //                             server_id, fsize, read_bytes,
            //                             track.current_byte);

            //                 while (v && !v->terminating
            //                        && v->get_secs_remaining () > 0.05f)
            //                     {
            //                         std::this_thread::sleep_for (
            //                             std::chrono::milliseconds (10));
            //                     }

            //                 // eof
            //                 if (!read_bytes)
            //                     break;
            //             }
            //     }
            // else
            //     {
            //         fprintf (stderr,
            //                  "[Manager::stream ERROR] Can't open file for "
            //                  "reading: %ld '%s'\n",
            //                  server_id, file_path.c_str ());
            //     }

            // oggz_close (track_og);

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
