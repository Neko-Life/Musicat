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

struct mc_oggz_user_data {
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

            OGGZ *track_og = oggz_open_stdio (ofile, OGGZ_READ);

            if (track_og)
                {
                    mc_oggz_user_data data = { v, track, debug };
                    oggz_set_read_callback (
                        track_og, -1,
                        [] (OGGZ *oggz, oggz_packet *packet, long serialno,
                            void *user_data) {
                            mc_oggz_user_data *data = (mc_oggz_user_data *)user_data;
                            data->voice_client->send_audio_opus (packet->op.packet,
                                                                 packet->op.bytes);

                            if (!data->track.seekable && packet->op.b_o_s == 0)
                                {
                                    data->track.seekable = true;
                                }

                            return 0;
                        },
                        (void *)&data);

                    size_t streamed_bytes = 0;

                    // stream loop
                    while (v && !v->terminating)
                        {
                            debug = get_debug_state ();

                            {
                                std::lock_guard<std::mutex> lk (this->sq_m);
                                auto sq = vector_find (&this->stop_queue, server_id);
                                if (sq != this->stop_queue.end ())
                                    {
                                        if (debug)
                                            printf ("[MANAGER::STREAM] "
                                                    "Stopping stream\n");
                                        break;
                                    }
                            }

                            static const constexpr long CHUNK_READ = BUFSIZ * 2;

                            const long read_bytes = oggz_read (track_og, CHUNK_READ);
                            streamed_bytes += read_bytes;

                            if (debug)
                                printf ("[Manager::stream] [guild_id] [size] "
                                        "[chunk] [read_bytes]: %ld %ld %ld %ld\n",
                                        server_id, fsize, read_bytes, streamed_bytes);

                            while (v && !v->terminating
                                   && v->get_secs_remaining () > 0.1)
                                {
                                    if (track.seek_to > 0)
                                        {
                                            oggz_seek (track_og, track.seek_to, SEEK_SET);
                                            track.seek_to = 0;
                                        }

                                    std::this_thread::sleep_for (
                                        std::chrono::milliseconds (30));
                                }

                            // eof
                            if (!read_bytes)
                                break;

                        }
                }
            else
                {
                    fprintf (
                        stderr,
                        "[Manager::stream ERROR] Can't open file for reading: %ld '%s'\n",
                        server_id, file_path.c_str ());
                }

            oggz_close (track_og);

            auto end_time = std::chrono::high_resolution_clock::now ();
            auto done = std::chrono::duration_cast<std::chrono::milliseconds> (
                end_time - start_time);
            if (debug)
                printf ("Done streaming for %ld milliseconds\n",
                        done.count ());
        }
    else
        throw 1;
}

} // player
} // musicat
