#include "musicat/player.h"
#include "musicat/db.h"
#include "musicat/mctrack.h"
#include "musicat/musicat.h"
#include <memory>

namespace musicat
{
namespace player
{
using string = std::string;

MCTrack::MCTrack () { this->init (); }

MCTrack::MCTrack (const YTrack &t)
{
    this->init ();
    this->raw = t.raw;

    if (t.raw.find ("raw_info") != t.raw.end ())
        this->info.raw = t.raw.value ("raw_info", nullptr);
}

MCTrack::~MCTrack () = default;

void
MCTrack::init ()
{
    seekable = false;
    seek_to = "";
    stopping = false;
    current_byte = 0;
    filesize = 0;
    repeat = 0;
}

bool
MCTrack::is_empty () const
{
    return filename.empty ();
}

void
Player::init ()
{
    this->guild_id = 0;
    this->cluster = nullptr;
    this->loop_mode = loop_mode_t::l_none;
    this->shifted_track = 0;
    this->info_message = nullptr;
    this->from = nullptr;
    this->auto_play = false;
    this->max_history_size = 0;
    this->stopped = false;
    this->channel_id = 0;
    this->saved_queue_loaded = false;
    this->saved_config_loaded = false;

    this->volume = 100;
    this->set_volume = -1;

    this->earwax = false;
    this->set_earwax = false;

    this->set_vibrato = false;
    this->vibrato_d = -1;
    this->vibrato_f = -1;

    this->set_tremolo = false;
    this->tremolo_d = -1;
    this->tremolo_f = -1;

    this->set_sampling_rate = false;
    this->sampling_rate = -1;

    this->tempo = 1.0;
    this->set_tempo = false;

    this->pitch = 0;
    this->set_pitch = false;

    this->set_equalizer = false;

    this->tried_continuing = false;

    this->processing_audio = false;
    this->opus_encoder = NULL;

    this->notification = true;
}

Player::Player () { this->init (); }

Player::Player (dpp::cluster *_cluster, const dpp::snowflake &_guild_id)
{
    this->init ();
    this->guild_id = _guild_id;
    this->cluster = _cluster;
}

Player::~Player ()
{
    this->loop_mode = loop_mode_t::l_none;
    this->shifted_track = 0;
    this->info_message = nullptr;
    this->from = nullptr;
    this->auto_play = false;
    this->max_history_size = 0;
    this->stopped = false;
    this->channel_id = 0;
    this->saved_queue_loaded = false;
    this->saved_config_loaded = false;
};

Player &
Player::add_track (MCTrack &track, bool top, const dpp::snowflake &guild_id,
                   const bool update_embed, const int64_t &arg_slip)
{
    size_t siz = 0;
    {
        // !TODO: remove this when fully using ytdlp to support non-yt tracks
        if (track.info.raw.is_null ())
            try
                {
                    track.info.raw
                        = yt_search::get_track_info (mctrack::get_url (track))
                              .audio_info (251)
                              .raw;

                    // track.thumbnails ();
                }
            catch (std::exception &e)
                {
                    std::cerr << "[Player::add_track ERROR] " << this->guild_id
                              << ':' << e.what () << '\n';

                    return *this;
                }

        siz = this->queue.size ();

        if (arg_slip == 1)
            top = true;

        if (top)
            {
                this->queue.push_front (track);
                if (this->shifted_track
                    < /* Check queue size after push_front */ (
                        this->queue.size () - 1))
                    {
                        this->shifted_track++;
                    }
            }

        // This is correct, don't "optimize" it
        else if (arg_slip > 1 && siz > (size_t)arg_slip)
            {
                this->queue.insert (this->queue.begin () + arg_slip, track);
            }
        else
            this->queue.push_back (track);
    }

    if (update_embed && siz > 0UL && guild_id && this->manager)
        this->manager->update_info_embed (guild_id);

    return *this;
}

Player &
Player::set_max_history_size (const size_t &siz)
{
    this->max_history_size = siz;
    int set = (int)siz;
    database::update_guild_player_config (this->guild_id, NULL, &set, NULL);
    return *this;
}

std::pair<std::deque<MCTrack>, int>
Player::skip (dpp::voiceconn *v)
{
    if (v && v->voiceclient)
        {
            /* const bool debug = get_debug_state (); */

            std::deque<MCTrack> removed_tracks = {};
            bool skipped = false;
            if (v->voiceclient->get_secs_remaining () > 0.05f)
                {
                    // if (this->queue.size()) {
                    //     removed_tracks.push_back (MCTrack
                    //     (this->queue.front())); if (get_debug_state ())
                    //         fprintf (stderr, "PUSHED FROM PLAYER SKIP:
                    //         '%s'\n",
                    //                 this->queue.front().title ().c_str ());
                    // }

                    v->voiceclient->pause_audio (false);
                    v->voiceclient->skip_to_next_marker ();

                    skipped = true;
                }

            if (this->is_stopped ())
                {
                    v->voiceclient->skip_to_next_marker ();
                    removed_tracks = this->skip_queue (1, false, true);
                    skipped = true;
                }

            if (skipped)
                {
                    return { removed_tracks, 0 };
                }
        }

    return { {}, -1 };
}

std::deque<MCTrack>
Player::skip_queue (int64_t amount, bool remove, bool pop_current,
                    bool push_back)
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

            this->queue.pop_front ();
            if (get_debug_state ())
                fprintf (stderr, "POPPED FROM QUEUE: '%s'\n",
                         mctrack::get_title (l).c_str ());

            removed_tracks.push_back (l);

            if (!remove && (push_back || l_s || l_q))
                {
                    l.repeat = 0;
                    this->queue.push_back (l);
                }
        }

    return removed_tracks;
}

Player &
Player::set_auto_play (const bool state)
{
    this->auto_play = state;
    database::update_guild_player_config (this->guild_id, &state, NULL, NULL);
    return *this;
}

bool
Player::reset_shifted ()
{
    if (this->queue.size () && this->shifted_track > 0)
        {
            auto i = this->queue.begin () + this->shifted_track;
            auto s = *i;
            this->queue.erase (i);
            this->queue.push_front (s);
            this->shifted_track = 0;
            return true;
        }

    this->shifted_track = 0;
    return false;
}

Player &
Player::set_loop_mode (int64_t mode)
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

Player &
Player::set_channel (const dpp::snowflake &channel_id)
{
    this->channel_id = channel_id;
    return *this;
}

size_t
Player::remove_track (const size_t &pos, size_t amount, const size_t &to)
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

            b = this->queue.erase (b);
            a++;
        }

    return amount;
}

size_t
Player::remove_track_by_user (const dpp::snowflake &user_id)
{
    if (!user_id)
        return 0;

    size_t ret = 0;
    auto i = this->queue.begin ();
    while (i != this->queue.end ())
        {
            if (i->user_id == user_id && i != this->queue.begin ())
                {
                    this->queue.erase (i);
                    ret++;
                    continue;
                }

            i++;
        }

    return ret;
}

bool
Player::pause (dpp::discord_client *from, const dpp::snowflake &user_id) const
{
    auto v = from->get_voice (guild_id);
    if (v && !v->voiceclient->is_paused ())
        {
            // !TODO: refactor to use status code!
            auto u = get_voice_from_gid (guild_id, user_id);
            if (!u.first)
                throw exception ("You're not in a voice channel", 1);

            if (u.first->id != v->channel_id)
                throw exception ("You're not in my voice channel", 0);

            v->voiceclient->pause_audio (true);
            // Paused
            return true;
        }

    // Not playing anythin
    return false;
}

bool
Player::shuffle (bool update_info_embed)
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
        this->queue.erase (this->queue.begin ());

        for (auto i : b)
            n_queue.push_back (this->queue.at (i));

        this->queue.clear ();

        this->queue = n_queue;
        this->queue.push_front (os);
    }

    if (update_info_embed)
        this->manager->update_info_embed (this->guild_id);
    return true;
}

void
Player::set_stopped (const bool &val)
{
    this->stopped = val;
}

bool
Player::is_stopped () const
{
    return this->stopped;
}

// ============================== FILTERS =============================

// methods to check if any filter is or should be active

bool
Player::fx_is_tempo_active () const
{
    return this->tempo != 1.0;
}

bool
Player::fx_is_pitch_active () const
{
    return this->pitch != 0;
}

bool
Player::fx_is_equalizer_active () const
{
    return !this->equalizer.empty ();
}

bool
Player::fx_is_sampling_rate_active () const
{
    return this->sampling_rate != -1;
}

bool
Player::fx_has_vibrato_f () const
{
    return this->vibrato_f != -1;
}

bool
Player::fx_has_vibrato_d () const
{
    return this->vibrato_d != -1;
}

bool
Player::fx_is_vibrato_active () const
{
    return this->fx_has_vibrato_f () || this->fx_has_vibrato_d ();
}

bool
Player::fx_has_tremolo_f () const
{
    return this->tremolo_f != -1;
}
bool
Player::fx_has_tremolo_d () const
{
    return this->tremolo_d != -1;
}

bool
Player::fx_is_tremolo_active () const
{
    return this->fx_has_tremolo_f () || this->fx_has_tremolo_d ();
}

bool
Player::fx_is_earwax_active () const
{
    return this->earwax;
}

// get active fx count
int
Player::fx_get_active_count () const
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
Player::load_fx_states (const nlohmann::json &fx_states)
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
        this->equalizer = i_equalizer->get<string> ();

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
Player::fx_states_to_json ()
{
    return {
        { "tempo", this->tempo },
        { "pitch", this->pitch },
        { "equalizer", this->equalizer },
        { "sampling_rate", this->sampling_rate },
        { "vibrato_f", this->vibrato_f },
        { "vibrato_d", this->vibrato_d },
        { "tremolo_f", this->tremolo_f },
        { "tremolo_d", this->tremolo_d },
        { "earwax", this->earwax },
    };
}

// ====================================================================

} // player

namespace util
{

bool
player_has_current_track (std::shared_ptr<player::Player> guild_player)
{
    if (!guild_player || guild_player->current_track.raw.is_null ()
        || !guild_player->queue.size ())
        return false;

    return true;
}

player::track_progress
get_track_progress (const player::MCTrack &track)
{
    int64_t duration = mctrack::get_duration (track);

    if (!duration || !track.filesize)
        return { 0, 0, 1 };

    float byte_per_ms = (float)track.filesize / (float)duration;

    int64_t current_ms = track.current_byte && byte_per_ms
                             ? (float)track.current_byte / byte_per_ms
                             : 0;

    return { current_ms, duration, 0 };
}

} // util
} // musicat

// vim: et ts=8 sw=4
