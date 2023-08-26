#include "musicat/audio_processing.h"
#include "musicat/musicat.h"
#include "musicat/player.h"
#include <oggz/oggz.h>
#include <oggz/oggz_seek.h>
#include <sys/stat.h>

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

            auto guild_player = get_player (v->server_id);
            if (!guild_player)
                throw 2;

            audio_processing::track_data_t p_track
                = { &track, file_path, guild_player };

            audio_processing::parent_child_ic_t p_info;
            audio_processing::run_processor_error_t status
                = audio_processing::run_processor (&p_track, &p_info);

            /* OGGZ *track_og = oggz_open_stdio (ofile, OGGZ_READ); */

            /* if (track_og) */
            /*     { */
            /*         mc_oggz_user_data data = { v, track, debug }; */

            /*         oggz_set_read_callback ( */
            /*             track_og, -1, */
            /*             [] (OGGZ *oggz, oggz_packet *packet, long serialno,
             */
            /*                 void *user_data) { */
            /*                 mc_oggz_user_data *data */
            /*                     = (mc_oggz_user_data *)user_data; */

            /*                 data->voice_client->send_audio_opus ( */
            /*                     packet->op.packet, packet->op.bytes); */

            /*                 if (!data->track.seekable && packet->op.b_o_s ==
             * 0) */
            /*                     { */
            /*                         data->track.seekable = true; */
            /*                     } */

            /*                 return 0; */
            /*             }, */
            /*             (void *)&data); */

            /*         // stream loop */
            /*         while (v && !v->terminating) */
            /*             { */
            /*                 if (is_stream_stopping (server_id)) */
            /*                     break; */

            /*                 debug = get_debug_state (); */

            /*                 static const constexpr long CHUNK_READ */
            /*                     = BUFSIZ * 2; */

            /*                 if (track.seek_to > -1 && track.seekable) */
            /*                     { */
            /*                         oggz_off_t offset = oggz_seek ( */
            /*                             track_og, track.seek_to, SEEK_SET);
             */

            /*                         if (debug) */
            /*                             fprintf ( */
            /*                                 stderr, */
            /*                                 "[Manager::stream] Seeking from:
             * " */
            /*                                 "%ld\nTarget: %ld\nResult
             * offset: " */
            /*                                 "%ld\n", */
            /*                                 track.current_byte,
             * track.seek_to, */
            /*                                 offset); */

            /*                         track.current_byte = offset; */
            /*                         track.seek_to = -1; */
            /*                     } */

            /*                 const long read_bytes */
            /*                     = oggz_read (track_og, CHUNK_READ); */

            /*                 track.current_byte += read_bytes; */

            /*                 if (debug) */
            /*                     printf ( */
            /*                         "[Manager::stream] [guild_id] [size] "
             */
            /*                         "[chunk] [read_bytes]: %ld %ld %ld
             * %ld\n", */
            /*                         server_id, fsize, read_bytes, */
            /*                         track.current_byte); */

            /*                 while (v && !v->terminating */
            /*                        && v->get_secs_remaining () > 0.05f) */
            /*                     { */
            /*                         std::this_thread::sleep_for ( */
            /*                             std::chrono::milliseconds (10)); */
            /*                     } */

            /*                 // eof */
            /*                 if (!read_bytes) */
            /*                     break; */
            /*             } */
            /*     } */
            /* else */
            /*     { */
            /*         fprintf (stderr, */
            /*                  "[Manager::stream ERROR] Can't open file for "
             */
            /*                  "reading: %ld '%s'\n", */
            /*                  server_id, file_path.c_str ()); */
            /*     } */

            /* oggz_close (track_og); */

            auto end_time = std::chrono::high_resolution_clock::now ();
            auto done = std::chrono::duration_cast<std::chrono::milliseconds> (
                end_time - start_time);

            if (debug)
                {
                    fprintf (stderr, "Done streaming for %ld milliseconds\n",
                             done.count ());
                    fprintf (stderr, "audio_processing status: %d\n", status);
                }

            if (status != audio_processing::SUCCESS)
                throw 2;
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

} // player
} // musicat
