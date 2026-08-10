// clang-format off
#include "musicat/mctrack.h"
#include "musicat/player.h"
// clang-format on

#include "musicat/server/ws/player.h"
#include "musicat/audio_config.h"
#include "musicat/cmds/filters.h"
#include "musicat/db.h"
#include "musicat/musicat.h"
#include "musicat/server/stream.h"
#include "musicat/util.h"

#include <cstddef>
#include <cstdint>
#include <memory>

#ifdef USING_STREAM_CODEC
#elif defined(USING_LIBOPUSENC)
#include "opusenc.h"
#else
#include "opus/opus.h"
#endif // USING_LIBOPUSENC

namespace musicat
{
namespace player
{

void
guild_player_t::init ()
{
    text_channel_id = 0;
    voice_channel_id = 0;
    info_message = nullptr;
    max_history_size = 1000;
    history.clear ();
    shifted_track = 0;
    queue.clear ();
    current_track = {};
    dashboard_control_requests.clear ();
    loop_mode = loop_mode_t::l_none;

    // fx_states ////////////////////////////////////////

    earwax = false;
    volume = 100;
    equalizer.clear ();
    sampling_rate = -1;
    pitch = 0;
    vibrato_d = -1;
    tremolo_d = -1;
    vibrato_f = -1;
    tremolo_f = -1;
    tempo = 1.0;

    // fx_states end ////////////////////////////////////////

    auto_play = true;
    saved_queue_loaded = false;
    saved_config_loaded = false;
    tried_continuing = false;
    processing_audio = false;
    notification = true;
    stopping = false;
    stopped = false;

#ifdef USING_STREAM_CODEC
    // encoder in stream_ctx for stream_codec
#elif defined(USING_LIBOPUSENC)
    opus_encoder = nullptr;
    opus_encoder_comments = nullptr;
#else
    opus_encoder = nullptr;
#endif
}

guild_player_t::guild_player_t (const dpp::snowflake &_guild_id, uint32_t _shard_id) : guild_id (_guild_id), shard_id (_shard_id)
{
    init ();
}

guild_player_t::~guild_player_t () { init (); };

std::lock_guard<std::mutex>
guild_player_t::acquire ()
{
    return std::lock_guard{ mutex };
}

dpp::discord_client *
guild_player_t::get_client ()
{
    if (shard_id == INVALID_SHARD_ID)
        return nullptr;

    return manager::get_client (shard_id);
}

static util::throttler_t add_track_throttler;

bool
guild_player_t::add_track_will_block (const MCTrack &track)
{
    return track.info.raw.is_null () && add_track_throttler.will_block ();
}

guild_player_t &
guild_player_t::add_track (MCTrack &track, bool top, const dpp::snowflake &guild_id, const bool update_embed, const int64_t &arg_slip)
{
    size_t siz = 0;
    {
        // !TODO: remove this when fully using ytdlp to support non-yt tracks
        if (track.info.raw.is_null ())
            try
                {
                    auto s = add_track_throttler.throttle ();

                    track.info.raw = yt_search::get_track_info (mctrack::get_url (track)).audio_info (251).raw;

                    // track.thumbnails ();
                }
            catch (std::exception &e)
                {
                    std::cerr << "[guild_player_t::add_track ERROR] " << this->guild_id << ':' << e.what () << '\n';

                    return *this;
                }

        siz = this->queue.size ();

        if (arg_slip == 1)
            top = true;

        if (top)
            {
                this->queue_add_front (track);
                if (this->shifted_track < /* Check queue size after push_front */ (this->queue.size () - 1))
                    {
                        this->shifted_track++;
                    }
            }

        // This is correct, don't "optimize" it
        else if (arg_slip > 1 && siz > (size_t)arg_slip)
            this->queue_insert (track, arg_slip);
        else
            this->queue_add (track);
    }

    if (update_embed && siz > 0UL && guild_id)
        manager::update_info_embed (guild_id);

    return *this;
}

guild_player_t &
guild_player_t::set_max_history_size (const size_t &siz)
{
    this->max_history_size = siz;
    int set = (int)siz;
    database::update_guild_player_config (this->guild_id, NULL, &set, NULL);
    return *this;
}

std::pair<std::deque<MCTrack>, int>
guild_player_t::skip_playback (dpp::voiceconn *v)
{
    if (!v || !v->voiceclient)
        return { {}, -1 };

    return skip_playback (v->voiceclient.get ());
}

std::pair<std::deque<MCTrack>, int>
guild_player_t::skip_playback (dpp::discord_voice_client *voiceclient)
{
    if (!voiceclient)
        return { {}, -1 };

    /* const bool debug = get_debug_state (); */

    auto ct = current_track;
    stop ();
    voiceclient->pause_audio (false);
    voiceclient->skip_to_next_marker ();

    if (get_debug_state ())
        {
            std::cerr << "stopped(" << stopped << ") "
                      << "processing_audio(" << processing_audio << ") "
                      << "stopping(" << stopping << ")\n";
        }

    return { { ct }, 0 };

    return { {}, -1 };
}

std::deque<MCTrack>
guild_player_t::skip_queue (int64_t amount, bool remove, bool pop_current, bool push_back)
{
    this->reset_shifted ();

    if (amount < 1)
        amount = 1;

    if (amount > 1000)
        amount = 1000;

    bool l_ = this->loop_mode == loop_mode_t::l_song;
    bool l_s = this->loop_mode == loop_mode_t::l_song_queue;
    bool l_q = this->loop_mode == loop_mode_t::l_queue;

    if (!this->current_track.is_empty ())
        this->current_track.repeat = 0;

    if (!this->queue.empty ())
        this->queue.front ().repeat = 0;

    bool should_pop_current_playback = pop_current || (l_ || l_s);

    std::deque<MCTrack> removed_tracks = {};
    for (int64_t i = should_pop_current_playback ? 0 : 1; i < amount; i++)
        {
            if (this->queue.begin () == this->queue.end ())
                break;

            MCTrack l = this->queue.front ();

            this->queue_pop_front ();
            if (get_debug_state ())
                fprintf (stderr, "POPPED FROM QUEUE: '%s'\n", mctrack::get_title (l).c_str ());

            removed_tracks.push_back (l);

            if (!remove && (push_back || l_s || l_q))
                {
                    l.repeat = 0;
                    this->queue_add (l);
                }
        }

    return removed_tracks;
}

guild_player_t &
guild_player_t::set_auto_play (const bool state)
{
    this->auto_play = state;
    database::update_guild_player_config (this->guild_id, &state, NULL, NULL);
    return *this;
}

bool
guild_player_t::reset_shifted ()
{
    if (this->queue.size () && this->shifted_track > 0)
        {
            auto i = this->queue.begin () + this->shifted_track;
            auto s = std::move (*i);
            this->queue_erase_i (i);
            this->queue_add_front (s);
            this->shifted_track = 0;
            return true;
        }

    this->shifted_track = 0;
    return false;
}

guild_player_t &
guild_player_t::set_loop_mode (int64_t mode)
{
    loop_mode_t nm = this->loop_mode;
    switch (mode)
        {
        case 0:
            nm = loop_mode_t::l_none;
            break;
        case 1:
            nm = loop_mode_t::l_song;
            break;
        case 2:
            nm = loop_mode_t::l_queue;
            break;
        case 3:
            nm = loop_mode_t::l_song_queue;
            break;
        }

    this->loop_mode = nm;
    database::update_guild_player_config (this->guild_id, NULL, NULL, &nm);

    return *this;
}

guild_player_t &
guild_player_t::set_channel (const dpp::snowflake &channel_id)
{
    text_channel_id = channel_id;
    return *this;
}

size_t
guild_player_t::remove_track (const size_t &pos, size_t amount, const size_t &to)
{
    if (!pos || (!amount && (long)to == -1))
        return 0;

    this->reset_shifted ();

    size_t siz = this->queue.size ();

    if ((pos + 1) > siz)
        return 0;

    if ((long)to != -1 && to > pos)
        amount = to - pos;

    size_t max = siz - pos;
    if (amount > max)
        amount = max;

    std::deque<MCTrack>::iterator b = this->queue.begin () + pos;
    size_t a = 0;

    while (b != this->queue.end ())
        {
            if (a == amount)
                break;

            b = this->queue_erase_i (b);
            a++;
        }

    return amount;
}

size_t
guild_player_t::remove_track_by_user (const dpp::snowflake &user_id)
{
    if (!user_id)
        return 0;

    size_t ret = 0;
    auto i = this->queue.begin ();
    while (i != this->queue.end ())
        {
            if (i->user_id == user_id && i != this->queue.begin ())
                {
                    i = this->queue_erase_i (i);
                    ret++;
                    continue;
                }

            i++;
        }

    return ret;
}

bool
guild_player_t::pause (dpp::discord_client *from, const dpp::snowflake &user_id)
{
    auto *v = get_voice_conn ();
    if (v && v->voiceclient && !v->voiceclient->is_paused ())
        {
            // !TODO: refactor to use status code!
            auto u = get_voice_from_gid (guild_id, user_id);
            if (!u.first)
                throw exception ("You're not in a voice channel", 1);

            if (u.first->id != v->channel_id)
                throw exception ("You're not in my voice channel", 0);

            v->voiceclient->pause_audio (true);
            // Paused
            server::ws::player::publish_pause (guild_id);
            return true;
        }

    // Not playing anythin
    return false;
}

bool
guild_player_t::shuffle (bool _update_info_embed)
{
    size_t siz = 0;
    {
        siz = this->queue.size ();
        if (siz < 3)
            return false;
    }

    this->reset_shifted ();

    std::deque<MCTrack> n_queue = {};
    auto b = shuffle_indexes (siz - 1);
    {
        MCTrack os = this->queue.at (0);
        this->queue_pop_front ();

        for (auto i : b)
            n_queue.push_back (this->queue.at (i));

        this->queue_clear ();

        this->set_queue (std::move (n_queue));
        this->queue_add_front (os);
    }

    if (_update_info_embed)
        manager::update_info_embed (this->guild_id);

    return true;
}

bool
guild_player_t::current_track_is_first_track () const
{
    return !queue.empty () && current_track.filename == queue.front ().filename;
}

guild_player_t &
guild_player_t::stop ()
{
    if (!processing_audio)
        return *this;

    auto vs = get_voice_from_gid (guild_id, get_sha_id ());
    if (!vs.first)
        return *this;

    stopping = true;
    current_track.current_byte = 0;
    reset_first_track_current_byte ();

    return *this;
}

std::pair<dpp::message, int>
guild_player_t::get_info_message ()
{
    if (!info_message.is_object ())
        return { {}, -1 };

    return { dpp::message ().fill_from_json (&info_message), 0 };
}

guild_player_t &
guild_player_t::set_info_message (const dpp::message &message)
{
    info_message = message.to_json (true, false);
    return *this;
}

#ifdef USING_STREAM_CODEC

static int
setup_encoder (guild_player_t *p)
{
    return 0;
}

static void
destroy_encoder (guild_player_t *p)
{
}

#elif defined(USING_LIBOPUSENC)

static void
handle_packet (void *guild_player, const unsigned char *packet_ptr, opus_int32 packet_len, opus_uint32 flags)
{
    if (!guild_player)
        return;

    dpp::discord_voice_client *vclient = ((guild_player_t *)guild_player)->get_voice_client ();
    if (!vclient)
        return;

    vclient->send_audio_opus (packet_ptr, packet_len, FRAME_DURATION);
}

static int
setup_encoder (guild_player_t *p)
{
    int status = 0;

    OpusEncCallbacks cbs;
    cbs.write = [] (void *guild_player, const unsigned char *page, opus_int32 len)
        {
            if (!guild_player)
                return 1;
            server::stream::broadcast (((guild_player_t *)guild_player)->guild_id, page, len);
            return 0;
        };

    cbs.close = [] (void *) { return 0; };

    p->opus_encoder_comments = ope_comments_create ();
    if (!p->opus_encoder_comments)
        {
            std::cerr << "[player::setup_encoder ERROR] "
                         "ope_comments_create() failure\n";

            p->opus_encoder_comments = NULL;
            return -1;
        }

    p->opus_encoder = ope_encoder_create_callbacks (&cbs, p, p->opus_encoder_comments, 48000, 2, 0, &status);

    if (!p->opus_encoder)
        {
            std::cerr << "[player::setup_encoder ERROR] "
                         "ope_encoder_create_callbacks() failure: "
                      << status << "\n";

            p->opus_encoder = NULL;

            if (p->opus_encoder_comments)
                {
                    ope_comments_destroy (p->opus_encoder_comments);
                    p->opus_encoder_comments = NULL;
                }

            return status;
        }

    int ret = ope_encoder_ctl (p->opus_encoder, OPE_SET_PACKET_CALLBACK (handle_packet, p));
    if (ret != OPE_OK)
        fprintf (stderr, "[player::setup_encoder WARN] OPE_SET_PACKET_CALLBACK: %s\n", ope_strerror (ret));

    // set frame duration to 60ms
    ret = ope_encoder_ctl (p->opus_encoder, OPUS_SET_EXPERT_FRAME_DURATION (OPUS_FRAMESIZE_60_MS));
    if (ret != OPE_OK)
        fprintf (stderr, "[player::setup_encoder WARN] OPUS_SET_EXPERT_FRAME_DURATION: %s\n", ope_strerror (ret));

    // set other configs used before
    ret = ope_encoder_ctl (p->opus_encoder, OPUS_SET_APPLICATION (OPUS_APPLICATION_AUDIO));
    if (ret != OPE_OK)
        fprintf (stderr, "[player::setup_encoder WARN] OPUS_SET_APPLICATION: %s\n", ope_strerror (ret));

    ret = ope_encoder_ctl (p->opus_encoder, OPUS_SET_SIGNAL (OPUS_SIGNAL_MUSIC));
    if (ret != OPE_OK)
        fprintf (stderr, "[player::setup_encoder WARN] OPUS_SET_SIGNAL: %s\n", ope_strerror (ret));

    return status;
}

static void
destroy_encoder (guild_player_t *p)
{
    if (p->opus_encoder)
        {
            ope_encoder_destroy (p->opus_encoder);
            p->opus_encoder = NULL;
        }

    if (p->opus_encoder_comments)
        {
            ope_comments_destroy (p->opus_encoder_comments);
            p->opus_encoder_comments = NULL;
        }
}

#else // USING_LIBOPUSENC

static int
setup_encoder (guild_player_t *p)
{
    int status = 0;

    p->opus_encoder = opus_encoder_create (48000, 2, OPUS_APPLICATION_AUDIO, &status);

    if (status != OPUS_OK)
        {
            std::cerr << "[player::setup_encoder ERROR] "
                         "opus_encoder_create() failure: "
                      << status << "\n";

            p->opus_encoder = NULL;

            return status;
        }

    if ((status = opus_encoder_ctl (p->opus_encoder, OPUS_SET_SIGNAL (OPUS_SIGNAL_MUSIC))) != OPUS_OK)
        {

            std::cerr << "[player::setup_encoder ERROR] "
                         "opus_encoder_ctl() failure: "
                      << status << "\n";

            opus_encoder_destroy (p->opus_encoder);
            p->opus_encoder = NULL;

            return status;
        }

    return status;
}

static void
destroy_encoder (guild_player_t *p)
{
    if (p->opus_encoder)
        {
            opus_encoder_destroy (p->opus_encoder);
            p->opus_encoder = NULL;
        }
}

#endif // USING_LIBOPUSENC

int
guild_player_t::init_for_stream ()
{
    int status = setup_encoder (this);

    if (status == 0)
        {
            reset_first_track_current_byte ();
            processing_audio = true;
        }

    return status;
}

guild_player_t &
guild_player_t::done_streaming ()
{
    destroy_encoder (this);

    stopping = false;
    processing_audio = false;
    // current_track.current_byte = 0;

    server::stream::unsubscribe (guild_id);

    return *this;
}

// ============================== FILTERS =============================

// methods to check if any filter is or should be active

bool
guild_player_t::fx_is_tempo_active () const
{
    return this->tempo != 1.0;
}

bool
guild_player_t::fx_is_pitch_active () const
{
    return this->pitch != 0;
}

bool
guild_player_t::fx_is_equalizer_active () const
{
    return !this->equalizer.empty ();
}

bool
guild_player_t::fx_is_sampling_rate_active () const
{
    return this->sampling_rate != -1;
}

bool
guild_player_t::fx_has_vibrato_f () const
{
    return this->vibrato_f != -1;
}

bool
guild_player_t::fx_has_vibrato_d () const
{
    return this->vibrato_d != -1;
}

bool
guild_player_t::fx_is_vibrato_active () const
{
    return this->fx_has_vibrato_f () || this->fx_has_vibrato_d ();
}

bool
guild_player_t::fx_has_tremolo_f () const
{
    return this->tremolo_f != -1;
}
bool
guild_player_t::fx_has_tremolo_d () const
{
    return this->tremolo_d != -1;
}

bool
guild_player_t::fx_is_tremolo_active () const
{
    return this->fx_has_tremolo_f () || this->fx_has_tremolo_d ();
}

bool
guild_player_t::fx_is_earwax_active () const
{
    return this->earwax;
}

// get active fx count
int
guild_player_t::fx_get_active_count () const
{
    int count = 0;

    if (this->fx_is_tempo_active ())
        count++;
    if (this->fx_is_pitch_active ())
        count++;
    if (this->fx_is_equalizer_active ())
        count++;
    if (this->fx_is_sampling_rate_active ())
        count++;
    if (this->fx_is_vibrato_active ())
        count++;
    if (this->fx_is_tremolo_active ())
        count++;
    if (this->fx_is_earwax_active ())
        count++;

    return count;
}

int
guild_player_t::load_fx_states (const nlohmann::json &fx_states)
{
    if (!fx_states.is_object ())
        return 1;

    const auto i_tempo = fx_states.find ("tempo");
    const auto i_pitch = fx_states.find ("pitch");
    const auto i_equalizer = fx_states.find ("equalizer");
    const auto i_sampling_rate = fx_states.find ("sampling_rate");
    const auto i_vibrato_f = fx_states.find ("vibrato_f");
    const auto i_vibrato_d = fx_states.find ("vibrato_d");
    const auto i_tremolo_f = fx_states.find ("tremolo_f");
    const auto i_tremolo_d = fx_states.find ("tremolo_d");
    const auto i_earwax = fx_states.find ("earwax");

    if (i_tempo != fx_states.end ())
        this->tempo = i_tempo->get<double> ();

    if (i_pitch != fx_states.end ())
        this->pitch = i_pitch->get<int> ();

    if (i_equalizer != fx_states.end ())
        {
            std::string eq = i_equalizer->get<std::string> ();
            auto arg = command::filters::af_args_to_equalizer_fx_t (eq);
            this->volume = arg.volume;
            this->equalizer = command::filters::equalizer_fx_t_to_af_args (arg, false);
        }

    if (i_sampling_rate != fx_states.end ())
        this->sampling_rate = i_sampling_rate->get<int64_t> ();

    if (i_vibrato_f != fx_states.end ())
        this->vibrato_f = i_vibrato_f->get<double> ();

    if (i_vibrato_d != fx_states.end ())
        this->vibrato_d = i_vibrato_d->get<int> ();

    if (i_tremolo_f != fx_states.end ())
        this->tremolo_f = i_tremolo_f->get<double> ();

    if (i_tremolo_d != fx_states.end ())
        this->tremolo_d = i_tremolo_d->get<int> ();

    if (i_earwax != fx_states.end ())
        this->earwax = i_earwax->get<bool> ();

    return 0;
}

nlohmann::json
guild_player_t::fx_states_to_json ()
{
    return {
        { "tempo", this->tempo },
        { "pitch", this->pitch },
        { "equalizer", this->equalizer + ",volume=" + std::to_string ((float)this->volume / 100) },
        { "sampling_rate", this->sampling_rate },
        { "vibrato_f", this->vibrato_f },
        { "vibrato_d", this->vibrato_d },
        { "tremolo_f", this->tremolo_f },
        { "tremolo_d", this->tremolo_d },
        { "earwax", this->earwax },
    };
}

static std::string
get_ffmpeg_pitch_args (int pitch, int64_t rel_sampling_rate, double rel_tempo)
{
    if (pitch == 0)
        return "";

    constexpr int64_t samp_per_percent = 24000 / 100;
    constexpr double tempo_per_percent = 0.5 / 100;

    int64_t sample = (rel_sampling_rate == -1 ? 48000 : rel_sampling_rate) + (pitch * (-samp_per_percent));
    double tempo = (rel_tempo == -1 ? 1.0 : rel_tempo) + ((double)pitch * (-tempo_per_percent));

    if (sample < 6000)
        sample = 6000;
    if (sample > 192000)
        sample = 192000;

    if (tempo < 0.5)
        tempo = 0.5;
    if (tempo > 3)
        tempo = 3.0;

    /*
        100=24000,0.5=-24000,-0.5=48000+(100*(-(24000/100))),1.0+(100*(-(0.5/100)))
        0=48000,1.0
        -100=72000,1.5=+24000,+0.5=48000+(-100*(-(24000/100))),1.0+(-100*(-(0.5/100)))
        -200=96000,2.0=+48000,+1.0=48000+(-200*(-(24000/100))),1.0+(-200*(-(0.5/100)))
        -300=120000,2.5=+72000,+1.5
        -400=144000,3.0=+96000,+2.0
    */

    return "aresample=" + std::to_string (sample) + ",atempo=" + std::to_string (tempo);
}

static std::string
get_ffmpeg_vibrato_args (bool has_f, bool has_d, guild_player_t *guild_player)
{
    std::string v_args;

    if (has_f)
        {
            v_args += "f=" + std::to_string (guild_player->vibrato_f);
        }

    if (has_d)
        {
            if (has_f)
                v_args += ':';

            int64_t nd = guild_player->vibrato_d;
            v_args += "d=" + std::to_string (nd > 0 ? (float)nd / 100 : nd);
        }

    return v_args;
}

static std::string
get_ffmpeg_tremolo_args (bool has_f, bool has_d, guild_player_t *guild_player)
{
    std::string v_args;

    if (has_f)
        {
            v_args += "f=" + std::to_string (guild_player->tremolo_f);
        }

    if (has_d)
        {
            if (has_f)
                v_args += ':';

            int64_t nd = guild_player->tremolo_d;
            v_args += "d=" + std::to_string (nd > 0 ? (float)nd / 100 : nd);
        }

    return v_args;
}

std::string
guild_player_t::get_filter_descr ()
{
    std::string descr = "volume=" + std::to_string ((double)volume / 100);

    if (fx_is_sampling_rate_active ())
        descr += ",aresample=" + std::to_string (sampling_rate);

    if (fx_is_tempo_active ())
        descr += ",atempo=" + std::to_string (tempo);

    if (fx_is_pitch_active ())
        descr += "," + get_ffmpeg_pitch_args (pitch, sampling_rate, tempo);

    if (fx_is_equalizer_active ())
        {
            if (equalizer.find ("volume") != equalizer.npos)
                fprintf (stderr, "[guild_player_t::get_filter_descr WARN] equalizer contains volume query!\n");
            descr += ",superequalizer=" + equalizer;
        }

    if (fx_is_vibrato_active ())
        {
            std::string v_args = get_ffmpeg_vibrato_args (fx_has_vibrato_f (), fx_has_vibrato_d (), this);
            descr += ",vibrato=" + v_args;
        }

    if (fx_is_tremolo_active ())
        {
            std::string v_args = get_ffmpeg_tremolo_args (fx_has_tremolo_f (), fx_has_tremolo_d (), this);
            descr += ",tremolo=" + v_args;
        }

    if (fx_is_earwax_active ())
        descr += ",earwax";

    return descr;
}

// ====================================================================

void
guild_player_t::check_for_to_seek ()
{
    if (queue.empty ())
        return;

    int64_t to_seek = current_track.current_byte - (BUFSIZ * 8);

    if (to_seek < 0)
        to_seek = 0;

    if (get_debug_state ())
        fprintf (stderr, "[guild_player_t::check_for_to_seek] to_seek(%ld)\n", to_seek);

    queue.front ().current_byte = to_seek;
}

void
guild_player_t::reset_first_track_current_byte ()
{
    if (!queue.empty ())
        {
            const bool debug = get_debug_state ();
            queue.front ().current_byte = 0;

            if (debug)
                std::cerr << "[guild_player_t::reset_first_track_current_byte] reset: title(" << mctrack::get_title (queue.front ())
                          << ")\n";
        }
}

dpp::voiceconn *
guild_player_t::get_voice_conn ()
{
    dpp::discord_client *pc = get_client ();
    if (pc == nullptr)
        return nullptr;

    auto *conn = pc->get_voice (guild_id);
    return conn;
}

dpp::discord_voice_client *
guild_player_t::get_voice_client ()
{
    auto *conn = get_voice_conn ();
    if (conn == nullptr)
        return nullptr;

    return conn->voiceclient.get ();
}

// queue operations
guild_player_t &
guild_player_t::queue_add (const MCTrack &t)
{
    const bool debug = get_debug_state ();
    if (debug)
        std::cerr << "[guild_player_t::queue_add] " << guild_id << ": `" << mctrack::get_title (t) << "`\n";

    queue.push_back (t);
    return *this;
}

guild_player_t &
guild_player_t::queue_pop ()
{
    const bool debug = get_debug_state ();
    if (debug)
        std::cerr << "[guild_player_t::queue_pop] " << guild_id << "\n";

    queue.pop_back ();
    return *this;
}

guild_player_t &
guild_player_t::queue_add_front (const MCTrack &t)
{
    const bool debug = get_debug_state ();
    if (debug)
        std::cerr << "[guild_player_t::queue_add_front] " << guild_id << ": `" << mctrack::get_title (t) << "`\n";

    queue.push_front (t);
    return *this;
}

guild_player_t &
guild_player_t::queue_pop_front ()
{
    const bool debug = get_debug_state ();
    if (debug)
        std::cerr << "[guild_player_t::queue_pop_front] " << guild_id << ": `" << mctrack::get_title (queue.front ()) << "\n";

    queue.pop_front ();
    return *this;
}

guild_player_t &
guild_player_t::queue_insert (const MCTrack &t, size_t pos)
{
    const bool debug = get_debug_state ();
    if (debug)
        std::cerr << "[guild_player_t::queue_insert] " << guild_id << ": `" << mctrack::get_title (t) << "` pos(" << pos << ")\n";

    queue.insert (queue.begin () + pos, t);
    return *this;
}

guild_player_t &
guild_player_t::queue_erase (size_t pos)
{
    const bool debug = get_debug_state ();
    if (debug)
        std::cerr << "[guild_player_t::queue_erase] " << guild_id << ": `" << mctrack::get_title (*(queue.begin () + pos)) << "` pos("
                  << pos << ")\n";

    queue.erase (queue.begin () + pos);
    return *this;
}

guild_player_t::track_queue::iterator
guild_player_t::queue_erase_i (track_queue::iterator i)
{
    const bool debug = get_debug_state ();
    if (debug)
        std::cerr << "[guild_player_t::queue_erase_i] " << guild_id << ": `" << mctrack::get_title (*i) << "`\n";

    return queue.erase (i);
}

guild_player_t &
guild_player_t::set_queue (const track_queue &q)
{
    const bool debug = get_debug_state ();
    if (debug)
        std::cerr << "[guild_player_t::set_queue] " << guild_id << " siz(" << q.size () << ")\n";

    queue = q;
    return *this;
}

guild_player_t &
guild_player_t::queue_clear ()
{
    const bool debug = get_debug_state ();
    if (debug)
        std::cerr << "[guild_player_t::queue_clear] " << guild_id << "\n";

    queue.clear ();
    return *this;
}

} // player
} // musicat

// vim: et ts=8 sw=4
