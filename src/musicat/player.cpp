#include "musicat/player.h"
#include "musicat/db.h"
#include "musicat/musicat.h"

namespace musicat
{
namespace player
{
using string = std::string;

MCTrack::MCTrack ()
{
    seekable = false;
    seek_to = -1;
    stopping = false;
    current_byte = 0;
    filesize = 0;
}

MCTrack::MCTrack (YTrack t)
{
    seekable = false;
    seek_to = -1;
    stopping = false;
    current_byte = 0;
    filesize = 0;
    this->raw = t.raw;
}

MCTrack::~MCTrack () = default;

Player::Player ()
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
}

Player::Player (dpp::cluster *_cluster, dpp::snowflake _guild_id)
{
    this->guild_id = _guild_id;
    this->cluster = _cluster;
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
Player::add_track (MCTrack track, bool top, dpp::snowflake guild_id,
                   bool update_embed, int64_t arg_slip)
{
    size_t siz = 0;
    {
        if (track.info.raw.is_null ())
            {
                track.info.raw = yt_search::get_track_info (track.url ())
                                     .audio_info (251)
                                     .raw;
                track.thumbnails ();
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
Player::set_max_history_size (size_t siz)
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
                    //     removed_tracks.push_back (MCTrack (this->queue.front()));
                    //     if (get_debug_state ())
                    //         fprintf (stderr, "PUSHED FROM PLAYER SKIP: '%s'\n",
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
                return { removed_tracks, 0 };
        }

    return { {}, -1 };
}

std::deque<MCTrack>
Player::skip_queue (int64_t amount, bool remove, bool pop_current)
{
    auto siz = this->queue.size ();
    if (amount < (siz || 1))
        amount = siz || 1;
    if (amount > 1000)
        amount = 1000;

    const bool l_s = this->loop_mode == loop_mode_t::l_song_queue;
    const bool l_q = this->loop_mode == loop_mode_t::l_queue;

    std::deque<MCTrack> removed_tracks = {};
    for (int64_t i
         = (pop_current || ((this->loop_mode == loop_mode_t::l_song) || l_s))
               ? 0
               : 1;
         i < amount; i++)
        {
            if (this->queue.begin () == this->queue.end ())
                break;

            MCTrack l = this->queue.front ();

            this->queue.pop_front ();
            if (get_debug_state ())
                fprintf (stderr, "POPPED FROM QUEUE: '%s'\n",
                         l.title ().c_str ());

            removed_tracks.push_back (l);

            if (!remove && (l_s || l_q))
                this->queue.push_back (l);
        }

    return removed_tracks;
}

Player &
Player::set_auto_play (bool state)
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
Player::set_loop_mode (int8_t mode)
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
Player::set_channel (dpp::snowflake channel_id)
{
    this->channel_id = channel_id;
    return *this;
}

size_t
Player::remove_track (size_t pos, size_t amount, const size_t to)
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
Player::remove_track_by_user (dpp::snowflake user_id)
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
                }
            else
                i++;
        }
    return ret;
}

bool
Player::pause (dpp::discord_client *from, dpp::snowflake user_id) const
{
    auto v = from->get_voice (guild_id);
    if (v && !v->voiceclient->is_paused ())
        {
            try
                {
                    auto u = get_voice_from_gid (guild_id, user_id);
                    if (u.first->id != v->channel_id)
                        throw exception ("You're not in my voice channel", 0);
                }
            catch (const char *e)
                {
                    throw exception ("You're not in a voice channel", 1);
                }
            v->voiceclient->pause_audio (true);
            // Paused
            return true;
        }
    // Not playing anythin
    else
        return false;
}

bool
Player::shuffle ()
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
    this->manager->update_info_embed (this->guild_id);
    return true;
}

// int Player::seek(int pos, bool abs) {
//     return 0;
// }

// int Player::stop() {
//     return 0;
// }

// int Player::resume() {
//     return 0;
// }

// int Player::search(string query) {
//     return 0;
// }

// int Player::join() {
//     return 0;
// }

// int Player::leave() {
//     return 0;
// }

// int Player::rejoin() {
//     return 0;
// }

void
Player::set_stopped (const bool &val)
{
    this->stopped = val;
}

const bool &
Player::is_stopped ()
{
    return this->stopped;
}
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
get_track_progress (player::MCTrack &track)
{
    const int64_t duration = track.info.duration ();

    if (!duration)
        return { 0, 0, 1 };

    float byte_per_ms = (float)track.filesize / (float)duration;

    const int64_t current_ms = track.current_byte && byte_per_ms
                                   ? (float)track.current_byte / byte_per_ms
                                   : 0;

    return { current_ms, duration, 0 };
}

} // util
} // musicat

// vim: et ts=8 sw=4
