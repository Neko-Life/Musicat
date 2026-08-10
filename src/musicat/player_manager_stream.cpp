// clang-format off
#include "musicat/mctrack.h"
#include "musicat/decoder.h"
#include "musicat/stream_codec.h"
#include "musicat/player.h"
#include "musicat/player_manager.h"
#include "musicat/player_manager_stream.h"
// clang-format on

#include "musicat/audio_config.h"
#include "musicat/db.h"
#include "musicat/musicat.h"
#include "musicat/player_manager_util.h"
#include "musicat/server/ws/player.h"
#include "musicat/thread_manager.h"
#include "musicat/util/fs.h"

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>

#if !defined(USING_STREAM_CODEC) && !defined(USING_LIBOPUSENC)
#include "opus_types.h"
#endif

#ifdef USING_STREAM_CODEC
#include "musicat/server/stream.h"
#endif // USING_STREAM_CODEC

#ifndef USING_STREAM_CODEC
#define SENDAUDIO() send_audio_routine (guild_player.get (), buffer, &read_size, false, guild_player->opus_encoder)
#else
#define SENDAUDIO() send_audio_routine (guild_player.get (), buffer, &read_size, false)
#endif

#define SEND_AUDIO_ROUTINE()                                                                                                               \
    do                                                                                                                                     \
        {                                                                                                                                  \
            if (debug)                                                                                                                     \
                {                                                                                                                          \
                    decode_end = std::chrono::high_resolution_clock::now ();                                                               \
                    encode_ts = decode_end;                                                                                                \
                }                                                                                                                          \
                                                                                                                                           \
            if (SENDAUDIO ())                                                                                                              \
                return -1;                                                                                                                 \
        }                                                                                                                                  \
    while (0)

namespace musicat::player::manager
{

struct handle_effect_chain_change_states_t
{
    std::shared_ptr<guild_player_t> &guild_player;
    MCTrack &track;
    decoder_t &dec;
};

static void
handle_effect_chain_change (handle_effect_chain_change_states_t states)
{
    const std::string new_descr = states.guild_player->get_filter_descr ();
    const bool filter_queried = new_descr != states.dec.get_filter_descr ();
    const bool seek_queried = !states.track.seek_to.empty ();
    if (filter_queried)
        {
            states.dec.set_filter_descr (new_descr);
            if (states.dec.init_filters () < 0)
                throw 2;

            database::update_guild_player_config (states.guild_player->guild_id, NULL, NULL, NULL,
                                                  states.guild_player->fx_states_to_json ());
            server::ws::player::publish_fx (states.guild_player->guild_id);
        }

    if (seek_queried)
        {
            states.dec.seek (util::get_track_progress (states.track).current_ms);
            states.track.seek_to = "";
            states.guild_player->reset_first_track_current_byte ();
        }
}

static void
calculate_track_progress (guild_player_t *guild_player, ssize_t *send_buffer_length)
{
    int64_t samp_calc = guild_player->sampling_rate == -1 ? 48000 : guild_player->sampling_rate;

    // take account earwax resampling
    if (guild_player->earwax)
        samp_calc -= 3900;

    // (buffer_size / (sampling rate * channel * (bit width(16) / bit per byte(8))) * 1 second in ms) * opus byte_per_ms
    int64_t add
        = (int64_t)((double)((float)((float)*send_buffer_length / (samp_calc * 2 * 2) * 1000) * opus_byte_per_ms) * guild_player->tempo);
    guild_player->current_track.current_byte += add;
}

#ifdef USING_STREAM_CODEC
static int
send_audio_routine (guild_player_t *guild_player, uint8_t *send_buffer, ssize_t *send_buffer_length, bool no_wait)
{
    // const bool debug = get_debug_state ();
    bool running_state = get_running_state ();

    auto *vclient = guild_player ? guild_player->get_voice_client () : nullptr;

    if (!running_state || !vclient || vclient->terminating)
        return 1;

    const bool debug = get_debug_state ();

    try
        {
            vclient->send_audio_opus (send_buffer, *send_buffer_length, FRAME_DURATION);
        }
    catch (const dpp::voice_exception &e)
        {
            fprintf (stderr, "[player::send_audio_routine ERROR] %s\n", e.what ());
        }

    *send_buffer_length = 0;

    return 0;
}

static int
write_packet (void *guild_id, const uint8_t *buf, int buf_size)
{
    if (!buf_size)
        return AVERROR_EOF;

    server::stream::broadcast (*(dpp::snowflake *)guild_id, buf, buf_size);

    return buf_size;
}

#elif defined(USING_LIBOPUSENC)

// this should be called
// inside the streaming thread
static int
send_audio_routine (guild_player_t *guild_player, uint8_t *send_buffer, ssize_t *send_buffer_length, bool no_wait, OggOpusEnc *opus_encoder)
{
    if (!opus_encoder)
        return 2;

    // const bool debug = get_debug_state ();
    bool running_state = get_running_state ();

    auto *vclient = guild_player ? guild_player->get_voice_client () : nullptr;

    if (!running_state || !vclient || vclient->terminating)
        return 1;

    const bool debug = get_debug_state ();

    // calculate duration
    if ((*send_buffer_length > 0))
        calculate_track_progress (guild_player, send_buffer_length);

    try
        {
            if (ope_encoder_write (opus_encoder, (opus_int16 *)send_buffer, (*send_buffer_length) / sizeof (opus_int16) / 2) != OPE_OK)
                return 3;
        }
    catch (const dpp::voice_exception &e)
        {
            fprintf (stderr, "[player::send_audio_routine ERROR] %s\n", e.what ());
        }

    *send_buffer_length = 0;

    return 0;
}

#else // USING_LIBOPUSENC

// this should be called
// inside the streaming thread
static int
send_audio_routine (guild_player_t *guild_player, uint8_t *send_buffer, ssize_t *send_buffer_length, bool no_wait,
                    OpusEncoder *opus_encoder)
{
    if (!opus_encoder)
        return 2;

    // const bool debug = get_debug_state ();
    bool running_state = get_running_state ();

    auto *vclient = guild_player ? guild_player->get_voice_client () : nullptr;

    if (!running_state || !vclient || vclient->terminating)
        return 1;

    const bool debug = get_debug_state ();

    if ((*send_buffer_length > 0))
        calculate_track_progress (guild_player, send_buffer_length);

    try
        {
            uint8_t pcmbuf[ENCODE_BUFFER_SIZE];
            ssize_t encoded_len = 0;
            while (encoded_len < *send_buffer_length)
                {
                    ssize_t remaining = *send_buffer_length - encoded_len;
                    ssize_t pcmbufsiz = ENCODE_BUFFER_SIZE > remaining ? remaining : ENCODE_BUFFER_SIZE;
                    memcpy (pcmbuf, send_buffer + encoded_len, pcmbufsiz);
                    encoded_len += pcmbufsiz;

                    if (pcmbufsiz < ENCODE_BUFFER_SIZE)
                        {
                            ssize_t trail = (ENCODE_BUFFER_SIZE - pcmbufsiz);
                            memset (pcmbuf + pcmbufsiz, 0, trail);

                            if (debug)
                                fprintf (stderr, "[player::send_audio_routine] Found last chunk of PCM buffer with size: %ld\n", pcmbufsiz);
                        }

                    uint8_t packet[OPUS_MAX_ENCODE_OUTPUT_SIZE];
                    int len = opus_encode (opus_encoder, (opus_int16 *)pcmbuf, FRAME_SIZE, packet, OPUS_MAX_ENCODE_OUTPUT_SIZE);

                    if (len < 0 || len > OPUS_MAX_ENCODE_OUTPUT_SIZE)
                        {
                            fprintf (stderr,
                                     "[player::send_audio_routine ERROR] "
                                     "opus_encode() returned %d\n",
                                     len);

                            return len;
                        }

                    if (len > 2)
                        {
                            vclient->send_audio_opus (packet, len, FRAME_DURATION);

                            // !TODO: use ogg_stream_t (need to build OpusHead and OpusTags headers manually)
                            // stream_codec::ogg_stream_t s (fd, stream_codec::OGG_STREAM_SUBMIT_OPUS_PACKET);
                        }
                }
        }
    catch (const dpp::voice_exception &e)
        {
            fprintf (stderr, "[player::send_audio_routine ERROR] %s\n", e.what ());
        }

    *send_buffer_length = 0;

    return 0;
}

#endif // USING_LIBOPUSENC

constexpr const char *fbsefmt = "[stream_ctx::end] Final buffer `%s`: %ld %ld\n";
constexpr const char *dssefmt = "[stream_ctx::end] Done streaming `%s` for %lld milliseconds\n";

class stream_ctx
{
    std::chrono::high_resolution_clock::time_point start_time;
    std::chrono::high_resolution_clock::time_point last_run_time;

    decoder_t dec;
#ifdef USING_STREAM_CODEC
    stream_codec::stream_codec_t streamc;
#endif // USING_STREAM_CODEC

  public:
    dpp::snowflake guild_id;
    bool handled;
    bool ended;

  private:
    bool running_state;
    bool is_stopping;

    float dpp_audio_buffer_length_second;

    ssize_t read_size = 0;
    ssize_t current_read = 0;
    ssize_t total_read = 0;
    uint8_t *buffer;
    std::vector<uint16_t> out;

    // benchmark purpose
    std::chrono::high_resolution_clock::time_point decode_ts, decode_end, encode_ts, encode_end;

    std::shared_ptr<guild_player_t>
    get_guild_player ()
    {
        return manager::get_player (guild_id);
    }

  public:
    stream_ctx (const dpp::snowflake &_guild_id) : guild_id (_guild_id), ended (true) {}
    ~stream_ctx () { end (); }

    stream_ctx () = delete;
    stream_ctx (const stream_ctx &) = delete;
    stream_ctx &operator= (const stream_ctx &) = delete;
    stream_ctx (stream_ctx &&) = delete;
    stream_ctx &operator= (stream_ctx &&) = delete;

    int
    init ()
    {
        handled = false;

        auto guild_player = get_guild_player ();
        if (!guild_player)
            throw 2;

        // voice_client is ready, mark continuing success
        guild_player->tried_continuing = false;

        MCTrack &track = guild_player->current_track;
        start_time = std::chrono::high_resolution_clock::now ();
        last_run_time = start_time;

        const std::string &fname = track.filename;
        const std::string music_folder_path = get_music_folder_path ();
        const std::string file_path = music_folder_path + fname;

        // check if we actually have the file
        util::fs::ensure_dir (music_folder_path);
        FILE *ofile = fopen (file_path.c_str (), "r");
        if (!ofile)
            throw 2;
        fclose (ofile);
        ofile = NULL;

        if (!dec.is_valid ())
            throw 2;

        if (dec.open (file_path.c_str ()) < 0)
            throw 2;

        dec.set_filter_descr (guild_player->get_filter_descr ());
        if (dec.init_filters () < 0)
            throw 2;

#ifdef USING_STREAM_CODEC
        if (streamc.init (&guild_id, &write_packet))
            throw 2;
#endif // USING_STREAM_CODEC

        // precondition done, play
        server::ws::player::publish_playback_info (guild_id);
        server::ws::player::publish_fx (guild_id);

        track.check_for_seek_to ();
        if (!track.seek_to.empty ())
            {
                const auto ms = util::get_track_progress (track).current_ms;
                dec.seek (ms);
                track.seek_to = "";
                server::ws::player::publish_seek (guild_id, ms);
                guild_player->reset_first_track_current_byte ();
            }

        dpp_audio_buffer_length_second = get_stream_buffer_size ();

        // I LOVE C++!!!
        running_state = false;
        is_stopping = false;

        // using raw pcm need to change ffmpeg output format to s16le!
        read_size = 0;
        current_read = 0;
        total_read = 0;
        buffer = nullptr;

        server::ws::player::publish_play (guild_id);
        ended = false;
        return 0;
    }

    int
    run ()
    {
        if (ended)
            return -1;

        auto guild_player = manager::get_player (guild_id);
        if (!guild_player)
            return -1;

        running_state = get_running_state ();
        is_stopping = guild_player->stopping;
        if (!running_state || is_stopping)
            return -1;

        if (manager::is_waiting_vc_ready (guild_id))
            return 0;

        auto *vclient = guild_player->get_voice_client ();
        if (!vclient || vclient->terminating)
            return -1;

        const bool debug = get_debug_state ();
        if (debug)
            {
                auto cur_time = std::chrono::high_resolution_clock::now ();

                auto delay_between_run_second = (float)((cur_time - last_run_time).count ()) / 1000000000;
                fprintf (stderr, "[player::manager::stream] delay_between_run_second(%f)\n", delay_between_run_second);

                last_run_time = cur_time;
                decode_ts = cur_time;
            }

        handle_effect_chain_change ({ guild_player, guild_player->current_track, dec });

#ifdef USING_STREAM_CODEC
        int ret;
        AVFrame *frm = nullptr;
        AVPacket *pkt = nullptr;
        while (!pkt && ret != AVERROR_EOF && ret >= 0)
            {
                while ((ret = streamc.get_packet (&pkt)) == AVERROR (EAGAIN))
                    {
                        ret = dec.process_frame (&frm);
                        if (ret == AVERROR_EOF || ret < 0)
                            {
                                // signal eof
                                streamc.write_pcm_frame (NULL);
                                break;
                            }

                        // samples * channel * size per sample
                        int n = frm->nb_samples * frm->ch_layout.nb_channels * sizeof (opus_int16);
                        calculate_track_progress (guild_player.get (), (ssize_t *)&n);

                        if (streamc.write_pcm_frame (frm) == AVERROR_EOF)
                            break;
                    }
            }

        if (ret == AVERROR_EOF || ret < 0)
            return -1;

        buffer = (uint8_t *)pkt->data;
        current_read = pkt->size;

        read_size += current_read;
        total_read += current_read;

        SEND_AUDIO_ROUTINE ();

        if ((ret = streamc.write_packet (pkt)) != 0)
            fprintf (stderr, "[stream_ctx::run WARN] streamc.write_packet returned (%d)\n", ret);

#elif defined(USING_LIBOPUSENC)
        int ret = dec.process_frame (out);
        if (ret == AVERROR_EOF || ret < 0)
            return -1;
        buffer = (uint8_t *)out.data ();
        current_read = out.size () * sizeof (uint16_t);

        read_size += current_read;
        total_read += current_read;

        SEND_AUDIO_ROUTINE ();

#else // USING_LIBOPUSENC
        read_size = out.size () * sizeof (uint16_t);
        std::vector<uint16_t> temp_out;
        int ret = dec.process_frame (temp_out);
        if (ret == AVERROR_EOF || ret < 0)
            return -1;
        out.insert (out.end (), temp_out.begin (), temp_out.end ());
        current_read = temp_out.size () * sizeof (uint16_t);

        read_size += current_read;
        total_read += current_read;
        if (read_size < ENCODE_BUFFER_SIZE)
            return 0;

        do
            {
                read_size = ENCODE_BUFFER_SIZE;
                buffer = (uint8_t *)out.data ();

                SEND_AUDIO_ROUTINE ();

                out.erase (out.begin (), out.begin () + (ENCODE_BUFFER_SIZE / sizeof (uint16_t)));
            }
        while ((read_size = out.size () * sizeof (uint16_t)) >= ENCODE_BUFFER_SIZE);
#endif

        if (debug)
            {
                encode_end = std::chrono::high_resolution_clock::now ();

                auto decode_latency = (decode_end - decode_ts).count ();
                auto encode_latency = (encode_end - encode_ts).count ();
                float total_latency_second = ((float)decode_latency / 1000000000) + ((float)encode_latency / 1000000000);

                fprintf (stderr, "[stream_ctx::run] decode_latency(%lldns) encode_latency(%lldns) total_latency_second(%f)\n",
                         decode_latency, encode_latency, total_latency_second);
            }

        return 0;
    }

    int
    need_handler ()
    {
        if (ended)
            return -1;

        auto guild_player = get_player (guild_id);
        if (!guild_player)
            return -1;

        if (manager::is_waiting_vc_ready (guild_id))
            return 0;

        auto *vclient = guild_player->get_voice_client ();
        if (!vclient || vclient->terminating)
            return -1;

        float outbuf_duration = vclient->get_secs_remaining ();
        if (get_debug_state ())
            fprintf (stderr, "[stream_ctx::need_handler] dpp_audio_buffer_length_second(%f) outbuf_duration(%f)\n",
                     dpp_audio_buffer_length_second, outbuf_duration);
        if ((outbuf_duration > dpp_audio_buffer_length_second))
            return 0;

        return 1;
    }

    int
    end ()
    {
        if (ended)
            return -1;
        ended = true;

        auto guild_player = get_guild_player ();
        if (!guild_player)
            return -1;

        const bool debug = get_debug_state ();
        MCTrack &track = guild_player->current_track;
        const std::string ttitle = mctrack::get_title (track);

        if ((read_size > 0) && running_state && !is_stopping)
            {
                if (debug)
                    fprintf (stderr, fbsefmt, ttitle.c_str (), (total_read += read_size), read_size);

                buffer = (uint8_t *)out.data ();
                SENDAUDIO ();
            }

#ifdef USING_STREAM_CODEC
        streamc.end ();
#elif defined(USING_LIBOPUSENC)
        ope_encoder_drain (guild_player->opus_encoder);
#endif // USING_LIBOPUSENC

        auto *vclient = guild_player->get_voice_client ();
        if (!running_state || is_stopping)
            {
                // clear voice client buffer
                if (vclient)
                    vclient->stop_audio ();

                server::ws::player::publish_stop (guild_id);
            }
        else if (!vclient && !guild_player->queue.empty ())
            {
                // somethins wrong, set last byte so we can continue playback later
                guild_player->queue.front ().current_byte = guild_player->current_track.current_byte;

                if (debug)
                    std::cerr << "[stream_ctx::end] set current_byte: guild_player->queue.front("
                              << mctrack::get_title (guild_player->queue.front ()) << ") current_byte("
                              << guild_player->current_track.current_byte << ")\n";
            }

        auto end_time = std::chrono::high_resolution_clock::now ();
        auto done = std::chrono::duration_cast<std::chrono::milliseconds> (end_time - start_time);

        if (debug)
            {
                fprintf (stderr, dssefmt, ttitle.c_str (), done.count ());

                if (vclient)
                    {
                        float outbuf_duration = vclient->get_secs_remaining ();
                        fprintf (stderr, "[stream_ctx::end] dpp_audio_buffer_length_second(%f) outbuf_duration(%f)\n",
                                 dpp_audio_buffer_length_second, outbuf_duration);
                    }
            }

        guild_player->done_streaming ();

        return 0;
    }
};

using vector_stream_ctx = std::vector<stream_ctx *>;
// protects ctx->handled and stream_ctxs
static exclusive_container<vector_stream_ctx> stream_ctxs;

static void
erase_ctx_unlocked (const dpp::snowflake &guild_id)
{
    auto i = stream_ctxs.get ().begin ();
    while (i != stream_ctxs.get ().end ())
        {
            if ((*i)->guild_id == guild_id)
                {
                    delete (*i);
                    stream_ctxs.get ().erase (i);
                    break;
                }

            i++;
        }
}

static void
erase_ctx (const dpp::snowflake &guild_id)
{
    auto lk = stream_ctxs.acquire ();
    erase_ctx_unlocked (guild_id);
}

class stream_thread_t
{
    std::vector<stream_ctx *> processing_ctxs;

  public:
    // protects ctxs_size and ctxs
    std::mutex t_m;
    std::condition_variable t_cv;
    std::vector<stream_ctx *> ctxs;

    // the size of processing_ctxs being processed
    size_t ctxs_size;

    stream_thread_t () : ctxs_size (0) {}

    void
    run ()
    {
        while (get_running_state ())
            {
                {
                    std::unique_lock lk (t_m);
                    processing_ctxs = std::move (ctxs);
                    ctxs_size = processing_ctxs.size ();
                    if (!ctxs_size)
                        {
                            // wait for ctx
                            t_cv.wait (lk);
                            continue;
                        }
                    // has ctx
                }

                for (auto *ctx : processing_ctxs)
                    {
                        bool erased = false;
                        do
                            {
                                if (!ctx->run ())
                                    continue;

                                auto guild_player = get_player (ctx->guild_id);

                                erase_ctx (ctx->guild_id);
                                erased = true;

                                auto *vclient = guild_player ? guild_player->get_voice_client () : nullptr;
                                if (vclient && !vclient->terminating)
                                    vclient->insert_marker ("e");
                            }
                        while (!erased && ctx->need_handler ());

                        if (erased)
                            continue;

                        {
                            auto lk = stream_ctxs.acquire ();
                            ctx->handled = false;
                        }
                    }

                // done handling
                processing_ctxs.clear ();
            }
    }

    void
    spawn (int i)
    {
        std::thread t (
            [i, this] ()
                {
                    thread_manager::DoneSetter tmds;
                    dpp::utility::set_thread_name ("mc/stream/" + std::to_string (i));
                    run ();
                });
        thread_manager::dispatch (t);
    }
};

static int stream_thread_count = 0;
static std::deque<stream_thread_t> stream_threads;

void
spawn_stream_thread (int count)
{
    if (stream_thread_count != 0)
        return;
    stream_thread_count = count;

    fprintf (stderr, "[player::spawn_stream_thread] Spawning %d stream thread\n", count);
    for (int i = 0; i < count; i++)
        stream_threads.emplace_back ().spawn (i);
}

static void
notify_work (stream_ctx *ctx)
{
    // current max and min processing size
    // this will be used in case all thread are currently busy
    size_t max = 0;
    size_t min = static_cast<size_t> (0) - 1;
    for (auto &t : stream_threads)
        {
            {
                std::lock_guard lk (t.t_m);
                if (t.ctxs_size)
                    {
                        // thread is processing ctxs, only check its size and continue
                        if (max < t.ctxs_size)
                            max = t.ctxs_size;
                        if (min > t.ctxs_size)
                            min = t.ctxs_size;

                        continue;
                    }
                // thread has no ctx and is idle
                t.ctxs.push_back (ctx);
            }
            t.t_cv.notify_one ();
            return;
        }

    // all thread are busy
    // slow path enqueueing unhandled ctx
    if (max == min)
        max++;
    for (auto &t : stream_threads)
        {
            {
                std::lock_guard lk (t.t_m);
                // pick thread with lower ctx size than max
                if (t.ctxs_size == max)
                    continue;
                t.ctxs.push_back (ctx);
            }
            t.t_cv.notify_one ();
            return;
        }
}

void
stream_shutdown ()
{
    for (auto &t : stream_threads)
        t.t_cv.notify_all ();
}

void
check_stream_contexts ()
{
    auto lk = stream_ctxs.acquire ();
    int notified = 0;
    for (auto *c : stream_ctxs.get ())
        {
            if (c->handled || !c->need_handler ())
                continue;

            notify_work (c);
            c->handled = true;
        }
}

void
submit_stream_ctx (const dpp::snowflake &guild_id)
{
    auto lk = stream_ctxs.acquire ();
    auto i = stream_ctxs.get ().begin ();
    while (i != stream_ctxs.get ().end ())
        {
            if ((*i)->guild_id == guild_id)
                throw 3;

            i++;
        }

    auto *ctx = new stream_ctx{ guild_id };
    try
        {
            ctx->init ();
        }
    catch (int e)
        {
            delete ctx;
            throw e;
        }
    catch (const std::exception &e)
        {
            delete ctx;

            fprintf (stderr, "[player::manager::submit_stream_ctx ERROR] %s\n", e.what ());
            throw 2;
        }

    stream_ctxs.get ().push_back (ctx);
}

} // musicat::player::manager
