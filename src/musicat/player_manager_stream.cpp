#include "musicat/musicat.h"
#include "musicat/player.h"
#include <ogg/ogg.h>
#include <sys/stat.h>

namespace musicat
{
namespace player
{
using string = std::string;

void
Manager::stream (dpp::discord_voice_client *v, string fname)
{
    dpp::snowflake server_id;
    std::chrono::_V2::system_clock::time_point start_time;

    const string music_folder_path = get_music_folder_path ();

    const bool debug = get_debug_state ();
    if (v && !v->terminating && v->is_ready ())
        {
            FILE *fd;
            ogg_sync_state oy;
            ogg_stream_state os;
            try
                {
                    server_id = v->server_id;

                    const string file_path = music_folder_path + fname;

                    start_time = std::chrono::high_resolution_clock::now ();

                    if (debug)
                        printf ("Streaming \"%s\" to %ld\n", fname.c_str (),
                                server_id);

                    fd = fopen (file_path.c_str (), "rb");
                    if (!fd)
                        {
                            std::filesystem::create_directory (
                                music_folder_path);
                            throw 2;
                        }

                    struct stat buf;
                    if (fstat (fileno (fd), &buf) != 0)
                        {
                            fclose (fd);
                            throw 2;
                        }

                    ogg_page og;
                    ogg_packet op;
                    // OpusHead header;

                    size_t sz = buf.st_size;
                    if (debug)
                        printf ("BUFFER_SIZE: %ld\n", sz);

                    ogg_sync_init (&oy);

                    // int eos = 0;
                    // int i;

                    fread (ogg_sync_buffer (&oy, sz), 1, sz, fd);
                    fclose (fd);
                    fd = NULL;

                    bool no_prob = true;

                    ogg_sync_wrote (&oy, sz);

                    if (ogg_sync_pageout (&oy, &og) != 1)
                        {
                            fprintf (stderr,
                                     "Does not appear to be ogg stream.\n");
                            no_prob = false;
                        }

                    ogg_stream_init (&os, ogg_page_serialno (&og));

                    if (ogg_stream_pagein (&os, &og) < 0)
                        {
                            fprintf (
                                stderr,
                                "Error reading initial page of ogg stream.\n");
                            no_prob = false;
                        }

                    if (ogg_stream_packetout (&os, &op) != 1)
                        {
                            fprintf (stderr, "Error reading header packet of "
                                             "ogg stream.\n");
                            no_prob = false;
                        }

                    /* We must ensure that the ogg stream actually contains
                     * opus data */
                    // if (!(op.bytes > 8 && !memcmp("OpusHead", op.packet,
                    // 8)))
                    // {
                    //     fprintf(stderr, "Not an ogg opus stream.\n");
                    //     exit(1);
                    // }

                    // /* Parse the header to get stream info */
                    // int err = opus_head_parse(&header, op.packet, op.bytes);
                    // if (err)
                    // {
                    //     fprintf(stderr, "Not a ogg opus stream\n");
                    //     exit(1);
                    // }
                    // /* Now we ensure the encoding is correct for Discord */
                    // if (header.channel_count != 2 &&
                    // header.input_sample_rate != 48000)
                    // {
                    //     fprintf(stderr, "Wrong encoding for Discord, must be
                    //     48000Hz sample rate with 2 channels.\n"); exit(1);
                    // }

                    /* Now loop though all the pages and send the packets to
                     * the vc */
                    bool br = false;
                    if (no_prob)
                        while (true)
                            {
                                if (br)
                                    {
                                        if (debug)
                                            printf ("[MANAGER::STREAM] "
                                                    "Stopping stream\n");
                                        break;
                                    }

                                {
                                    std::lock_guard<std::mutex> lk (
                                        this->sq_m);
                                    auto sq = vector_find (&this->stop_queue,
                                                           server_id);
                                    if (sq != this->stop_queue.end ())
                                        break;
                                }

                                if (!v || v->terminating)
                                    {
                                        fprintf (stderr,
                                                 "[ERROR MANAGER::STREAM] "
                                                 "Can't continue streaming, "
                                                 "connection broken\n");
                                        break;
                                    }

                                if (ogg_sync_pageout (&oy, &og) != 1)
                                    {
                                        // fprintf(stderr, "[ERROR
                                        // MANAGER::STREAM] Can't continue
                                        // streaming, corrupt audio (need
                                        // recapture or incomplete audio
                                        // file)\n");
                                        break;
                                    }

                                if (ogg_stream_pagein (&os, &og) < 0)
                                    {
                                        fprintf (stderr,
                                                 "[ERROR MANAGER::STREAM] "
                                                 "Can't continue streaming, "
                                                 "error reading page of Ogg "
                                                 "bitstream data\n");
                                        break;
                                    }

                                int po_res;
                                while (
                                    (po_res = ogg_stream_packetout (&os, &op)))
                                    {
                                        if (po_res == -1)
                                            {
                                                fprintf (
                                                    stderr,
                                                    "[WARN MANAGER::STREAM] "
                                                    "Audio gap\n");
                                            }
                                        if (po_res == 0)
                                            {
                                                fprintf (
                                                    stderr,
                                                    "[WARN MANAGER::STREAM] "
                                                    "Async or delayed "
                                                    "audio\n");
                                                break;
                                            }

                                        // /* Read remaining headers */
                                        // if (op.bytes > 8 &&
                                        // !memcmp("OpusHead", op.packet, 8))
                                        // {
                                        //     int err =
                                        //     opus_head_parse(&header,
                                        //     op.packet, op.bytes); if (err)
                                        //     {
                                        //         fprintf(stderr, "Not a ogg
                                        //         opus stream\n"); exit(1);
                                        //     }
                                        //     if (header.channel_count != 2 &&
                                        //     header.input_sample_rate !=
                                        //     48000)
                                        //     {
                                        //         fprintf(stderr, "Wrong
                                        //         encoding for Discord, must
                                        //         be 48000Hz sample rate with
                                        //         2 channels.\n"); exit(1);
                                        //     }
                                        //     continue;
                                        // }
                                        /* Skip the opus tags */
                                        /* if (op.bytes > 8 &&
                                         * !memcmp("OpusTags", op.packet, 8))
                                         */
                                        /* continue; */

                                        /* Send the audio */
                                        // int samples =
                                        // opus_packet_get_samples_per_frame(op.packet,
                                        // 48000);

                                        if (v && !v->terminating)
                                            v->send_audio_opus (
                                                op.packet, op.bytes,
                                                20); // samples / 48);
                                        /* if (ogg_stream_eos(&os)) */
                                        /*     break; */

                                        br = v->terminating;

                                        while (v && !v->terminating
                                               && v->get_secs_remaining ()
                                                      > 0.5)
                                            {
                                                std::this_thread::sleep_for (
                                                    std::chrono::milliseconds (
                                                        250));
                                                if (v->terminating)
                                                    br = true;
                                            }

                                        {
                                            std::lock_guard<std::mutex> lk (
                                                this->sq_m);
                                            auto sq = vector_find (
                                                &this->stop_queue, server_id);
                                            if (sq != this->stop_queue.end ())
                                                {
                                                    br = true;
                                                    break;
                                                }
                                        }

                                        if (br)
                                            {
                                                if (debug)
                                                    printf (
                                                        "[MANAGER::STREAM] "
                                                        "Stopping stream\n");
                                                break;
                                            }
                                    }
                            }
                }
            catch (const std::system_error &e)
                {
                    fprintf (stderr, "[ERROR MANAGER::STREAM] %d: %s\n",
                             e.code ().value (), e.what ());
                }
            /* Cleanup */
            ogg_stream_clear (&os);
            ogg_sync_clear (&oy);
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
