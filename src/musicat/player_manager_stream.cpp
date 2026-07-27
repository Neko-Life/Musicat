#include "musicat/audio_config.h"
#include "musicat/db.h"
#include "musicat/decoder.h"
#include "musicat/mctrack.h"
#include "musicat/musicat.h"
#include "musicat/player.h"
#include "musicat/server/ws/player.h"
#include "musicat/thread_manager.h"
#include "musicat/util/fs.h"
#include "opus_types.h"

#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>

namespace musicat::player
{

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

#ifdef USING_LIBOPUSENC

// this should be called
// inside the streaming thread
int
send_audio_routine (Player *guild_player, uint8_t *send_buffer, ssize_t *send_buffer_length, bool no_wait, OggOpusEnc *opus_encoder)
{
    // const bool debug = get_debug_state ();
    bool running_state = get_running_state ();

    auto *vclient = guild_player ? guild_player->get_voice_client () : nullptr;

    if (!running_state || !vclient || vclient->terminating)
        return 1;

    const bool debug = get_debug_state ();

    // calculate duration
    if ((*send_buffer_length > 0))
        {
            int64_t samp_calc = guild_player->sampling_rate == -1 ? 48000 : guild_player->sampling_rate;

            // take account earwax resampling
            if (guild_player->earwax)
                samp_calc -= 3900;

            // (buffer_size / (sampling rate * channel * (bit width(16) / bit per byte(8))) * 1 second in ms) * opus byte_per_ms
            int64_t add = (int64_t)((double)((float)((float)*send_buffer_length / (samp_calc * 2 * 2) * 1000) * opus_byte_per_ms)
                                    * guild_player->tempo);
            guild_player->current_track.current_byte += add;
        }

    try
        {
            if (!opus_encoder)
                return 2;

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
int
send_audio_routine (dpp::discord_voice_client *vclient, uint16_t *send_buffer, ssize_t *send_buffer_length, bool no_wait,
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
            auto guild_player = player_manager ? player_manager->get_player (vclient->server_id) : NULL;

            if (guild_player)
                {
                    int64_t samp_calc = guild_player->sampling_rate == -1 ? 48000 : guild_player->sampling_rate;

                    // take account earwax resampling
                    if (guild_player->earwax)
                        samp_calc -= 3900;

                    // (buffer_size / (sampling rate * channel * (bit width(16) / bit per byte(8))) * 1 second in ms) * opus byte_per_ms
                    int64_t add = (int64_t)((double)((float)((float)*send_buffer_length / (samp_calc * 2 * 2) * 1000) * opus_byte_per_ms)
                                            * guild_player->tempo);
                    guild_player->current_track.current_byte += add;
                }
        }

    try
        {
            if (!opus_encoder)
                return 2;

            std::vector<uint16_t> pcmbuf (send_buffer, send_buffer + *send_buffer_length);
            while (!pcmbuf.empty ())
                {
                    uint8_t packet[OPUS_MAX_ENCODE_OUTPUT_SIZE];

                    const auto pbufsiz = pcmbuf.size ();
                    if (pbufsiz < ENCODE_BUFFER_SIZE)
                        {
                            if (debug)
                                {
                                    fprintf (stderr,
                                             "[player::send_audio_"
                                             "routine] Found last chunk of "
                                             "PCM buffer with size: %ld\n",
                                             pbufsiz);
                                }

                            pcmbuf.resize (ENCODE_BUFFER_SIZE);
                        }

                    int len = opus_encode (opus_encoder, (opus_int16 *)pcmbuf.data (), FRAME_SIZE, packet, OPUS_MAX_ENCODE_OUTPUT_SIZE);

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

                    pcmbuf.erase (pcmbuf.begin (), pcmbuf.begin () + ENCODE_BUFFER_SIZE);
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

constexpr const char *msprrfmt = "[Manager::stream ERROR] Processor not ready or exited: %s\n";
constexpr const char *fbsefmt = "[Manager::stream] Final buffer `%s`: %ld %ld\n";
constexpr const char *dssefmt = "[Manager::stream] Done streaming `%s` for %lld milliseconds\n";

void
Manager::stream (const dpp::snowflake &guild_id)
{
}

class stream_ctx
{
    std::chrono::high_resolution_clock::time_point start_time;
    std::chrono::high_resolution_clock::time_point last_run_time;

    decoder_t dec;

  public:
    dpp::snowflake guild_id;
    bool handled;

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

    std::shared_ptr<Player>
    get_guild_player ()
    {
        auto *player_manager = get_player_manager_ptr ();
        return player_manager ? player_manager->get_player (guild_id) : nullptr;
    }

  public:
    stream_ctx (const dpp::snowflake &_guild_id) : guild_id (_guild_id) {}

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
        return 0;
    }

    int
    run ()
    {
        auto *player_manager = get_player_manager_ptr ();
        auto guild_player = player_manager ? player_manager->get_player (guild_id) : nullptr;
        if (!guild_player)
            return -1;

        running_state = get_running_state ();
        is_stopping = guild_player->stopping;
        if (!running_state || is_stopping)
            return -1;

        if (player_manager->is_waiting_vc_ready (guild_id))
            return 0;

        auto *vclient = guild_player->get_voice_client ();
        if (!vclient || vclient->terminating)
            return -1;

        const bool debug = get_debug_state ();
        if (debug)
            {
                auto cur_time = std::chrono::high_resolution_clock::now ();

                auto delay_between_run_second = (float)((cur_time - last_run_time).count ()) / 1000000000;
                fprintf (stderr, "[Manager::stream] delay_between_run_second(%f)\n", delay_between_run_second);

                last_run_time = cur_time;
                decode_ts = cur_time;
            }

        handle_effect_chain_change ({ guild_player, guild_player->current_track, dec });

        int ret = dec.process_frame (out);
        if (ret == AVERROR_EOF || ret < 0)
            return -1;
        buffer = (uint8_t *)out.data ();
        current_read = out.size () * sizeof (uint16_t);

        read_size += current_read;
        total_read += current_read;

        if (debug)
            {
                decode_end = std::chrono::high_resolution_clock::now ();
                encode_ts = decode_end;
            }

        if (send_audio_routine (guild_player.get (), buffer, &read_size, false, guild_player->opus_encoder))
            return -1;

        if (debug)
            {
                encode_end = std::chrono::high_resolution_clock::now ();

                auto decode_latency = (decode_end - decode_ts).count ();
                auto encode_latency = (encode_end - encode_ts).count ();
                float total_latency_second = ((float)decode_latency / 1000000000) + ((float)encode_latency / 1000000000);

                fprintf (stderr, "[Manager::stream] decode_latency(%lldns) encode_latency(%lldns) total_latency_second(%f)\n",
                         decode_latency, encode_latency, total_latency_second);
            }

        return 0;
    }

    int
    need_handler ()
    {
        auto *player_manager = get_player_manager_ptr ();
        auto guild_player = player_manager ? player_manager->get_player (guild_id) : nullptr;
        if (!guild_player)
            return -1;

        if (player_manager->is_waiting_vc_ready (guild_id))
            return 0;

        auto *vclient = guild_player->get_voice_client ();
        if (!vclient || vclient->terminating)
            return -1;

        float outbuf_duration = vclient->get_secs_remaining ();
        if ((outbuf_duration > dpp_audio_buffer_length_second))
            return 0;

        return 1;
    }

    int
    end ()
    {
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
            }

#ifdef USING_LIBOPUSENC
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
                    std::cerr << "[Manager::stream] set current_byte: guild_player->queue.front("
                              << mctrack::get_title (guild_player->queue.front ()) << ") current_byte("
                              << guild_player->current_track.current_byte << ")\n";
            }

        auto end_time = std::chrono::high_resolution_clock::now ();
        auto done = std::chrono::duration_cast<std::chrono::milliseconds> (end_time - start_time);

        if (debug)
            fprintf (stderr, dssefmt, ttitle.c_str (), done.count ());

        guild_player->done_streaming ();
        return 0;
    }
};

static std::vector<stream_ctx *> stream_ctxs;
// protects ctx->handled and stream_ctxs
static std::mutex stream_ctxs_m;

static void
erase_ctx_unlocked (const dpp::snowflake &guild_id)
{
    auto i = stream_ctxs.begin ();
    while (i != stream_ctxs.end ())
        {
            if ((*i)->guild_id == guild_id)
                {
                    delete (*i);
                    stream_ctxs.erase (i);
                    break;
                }

            i++;
        }
}

static void
erase_ctx (const dpp::snowflake &guild_id)
{
    std::lock_guard lk (stream_ctxs_m);
    erase_ctx_unlocked (guild_id);
}

struct stream_thread_t
{
    // protects this->ctx
    std::mutex t_m;
    std::condition_variable t_cv;

    stream_ctx *ctx;

    stream_thread_t () : ctx (nullptr) {}

    void
    run ()
    {
        while (get_running_state ())
            {
                {
                    std::unique_lock lk (t_m);
                    if (!ctx)
                        {
                            // wait for ctx
                            t_cv.wait (lk);
                            continue;
                        }
                }

                while (ctx && ctx->need_handler ())
                    {
                        if (!ctx->run ())
                            continue;
                        ctx->end ();

                        auto *player_manager = get_player_manager_ptr ();
                        auto guild_player = player_manager ? player_manager->get_player (ctx->guild_id) : nullptr;

                        erase_ctx (ctx->guild_id);
                        {
                            std::lock_guard lk (t_m);
                            ctx = nullptr;
                        }

                        auto *vclient = guild_player ? guild_player->get_voice_client () : nullptr;
                        if (vclient && !vclient->terminating)
                            vclient->insert_marker ("e");
                    }

                if (!ctx)
                    continue;

                {
                    std::lock_guard lk (stream_ctxs_m);
                    ctx->handled = false;
                }
                std::lock_guard lk (t_m);
                ctx = nullptr;
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
    for (auto &t : stream_threads)
        {
            {
                std::lock_guard lk (t.t_m);
                if (t.ctx)
                    continue;
                t.ctx = ctx;
            }
            t.t_cv.notify_one ();

            ctx->handled = true;
            break;
        }

    if (!ctx->handled)
        fprintf (stderr, "[player::notify_work WARN] !!! All stream thread occupied, cannot handle stream context: %s !!!\n",
                 ctx->guild_id.str ().c_str ());
}

void
shutdown ()
{
    for (auto &t : stream_threads)
        t.t_cv.notify_all ();
}

void
check_stream_contexts ()
{
    std::lock_guard lk (stream_ctxs_m);
    int notified = 0;
    for (auto *c : stream_ctxs)
        {
            if (c->handled || !c->need_handler ())
                continue;

            notify_work (c);
        }
}

void
Manager::submit_stream_ctx (const dpp::snowflake &guild_id)
{
    std::lock_guard lk (stream_ctxs_m);
    auto i = stream_ctxs.begin ();
    while (i != stream_ctxs.end ())
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

    stream_ctxs.push_back (ctx);
}

void
Manager::stream_noslave (const dpp::snowflake &guild_id)
{
}

void
Manager::set_processor_state (std::string &server_id_str, processor_state_t state)
{
    std::lock_guard lk (this->as_m);

    this->processor_states[server_id_str] = state;
}

processor_state_t
Manager::get_processor_state (std::string &server_id_str)
{
    std::lock_guard lk (this->as_m);
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

} // musicat::player
