#ifndef SHA_PLAYER_H
#define SHA_PLAYER_H

#include "mctrack.h"
#include "musicat/audio_config.h"
#include "opus_types.h"

#ifdef USING_STREAM_CODEC

#elif defined(USING_LIBOPUSENC)
#include "opusenc.h"
#else
#include "opus/opus.h"
#endif // USING_LIBOPUSENC

#include <cstdint>
#include <deque>
#include <dpp/dpp.h>
#include <mutex>
#include <string>

namespace musicat::player
{

// ms * (48kHz/1s) * channels * sample_size
inline constexpr int64_t opus_byte_per_ms = 1 * 48 * 2 * sizeof (opus_int16);

/**
 * Component Ids
 */
constexpr struct
{
    const char *pause = "playnow/p";
    const char *resume = "playnow/r";
    const char *stop = "playnow/s";
    const char *loop = "playnow/l";
    const char *shuffle = "playnow/h";

    const char *expand = "playnow/e";
    const char *unexpand = "playnow/x";

    const char *prev = "playnow/v";
    const char *rewind = "playnow/w";
    const char *autoplay = "playnow/a";
    const char *forward = "playnow/f";
    const char *next = "playnow/n";

    const char *disable_notif = "playnow/d";
    const char *enable_notif = "playnow/b";
    const char *update = "playnow/u";
} ids;

enum loop_mode_t
{
    // No looping
    l_none,
    // Loop song
    l_song,
    // Loop queue
    l_queue,
    // Loop song and queue
    l_song_queue
};

// ================================================================================

// this class only ever created on join/play cmd
struct guild_player_t
{
    using track_queue = std::deque<MCTrack>;

    static const uint32_t INVALID_SHARD_ID = 0xFFFFFFFF;

    /**
     * @brief Guild ID this player belongs to.
     * Player is only deleted when Musicat left the server
     * or not in vc for at least 1 hour.
     *
     * Should only be set when instantiating.
     */
    dpp::snowflake guild_id;

    /**
     * @brief Shard this guild player belongs to.
     *
     * Should only be set when instantiating.
     */
    uint32_t shard_id;

    /**
     * @brief Text channel ID to send now playing embed.
     *
     */
    dpp::snowflake text_channel_id;

    /**
     * @brief Voice channel ID this player is currently attached to.
     *
     */
    dpp::snowflake voice_channel_id;

    /**
     * @brief Thread safety mutex. Must lock this whenever updating state.
     * Can use the provided helper method acquire() to lock this.
     */
    std::mutex mutex;

    /**
     * @brief Message info of currently playing song.
     *
     */
    nlohmann::json info_message;

    /**
     * @brief History size limiter
     *
     */
    size_t max_history_size;

    /**
     * @brief Played song history containing song Ids
     *
     */
    std::deque<std::string> history;

    /**
     * @brief Number of added track to the front of queue.
     *
     */
    size_t shifted_track;

    /**
     * @brief Track queue.
     *
     */
    track_queue queue;

    /**
     * @brief Current track stream
     *
     */
    MCTrack current_track;

    /**
     * @brief Loop mode of the currently playing song.
     *
     * 0: Not looping,
     * 1: Looping one song and remove skipped song,
     * 2: Looping queue,
     * 3: Looping one song and won't remove skipped song.
     *
     */
    loop_mode_t loop_mode;

    // fx_states ////////////////////////////////////////

    // 3 byte padding here
    bool earwax;

    // default 100
    int volume;

    /**
     * @brief Equalizer raw ffmpeg opt.
     */
    std::string equalizer;

    /**
     * @brief Default -1
     */
    int sampling_rate;

    // -400-100, default 0
    int pitch;

    /**
     * @brief In percent, so it should be converted with (float)d/100 when
     * provided to ffmpeg, default -1
     */
    int vibrato_d;

    /**
     * @brief In percent, so it should be converted with (float)d/100 when
     * provided to ffmpeg, default -1
     */
    int tremolo_d;

    // default -1
    double vibrato_f;
    // default -1
    double tremolo_f;
    // 0.5-4.0, default 1.0
    double tempo;

    // fx_states end ////////////////////////////////////////

    /**
     * @brief Whether auto play is enabled for this player
     *
     */
    bool auto_play;

    /**
     * @brief Whether this player already tried to load saved queue after boot.
     */
    bool saved_queue_loaded;

    /**
     * @brief Whether this player already tried to load saved config after
     * boot.
     */
    bool saved_config_loaded;

    bool tried_continuing;

    /**
     * @brief Is processing audio?
     */
    bool processing_audio;

    /**
     * @brief Is notification enabled?
     */
    bool notification;

    /**
     * @brief Is currently stopping its stream?
     */
    bool stopping;

    /**
     * @brief Is currently stopped (stop cmd issued)?
     */
    bool stopped;

#ifdef USING_STREAM_CODEC
    // encoder in stream_ctx for stream_codec
#elif defined(USING_LIBOPUSENC)
    OggOpusEnc *opus_encoder;
    OggOpusComments *opus_encoder_comments;
#else
    OpusEncoder *opus_encoder;
#endif

    void init ();

    guild_player_t (const dpp::snowflake &_guild_id, uint32_t shard_id);
    ~guild_player_t ();

    std::lock_guard<std::mutex> acquire ();

    dpp::discord_client *get_client ();

    // see if adding this track will block
    static bool add_track_will_block (const MCTrack &track);

    guild_player_t &add_track (MCTrack &track, bool top = false, const dpp::snowflake &guild_id = 0, const bool update_embed = true,
                               const int64_t &arg_slip = 0);

    guild_player_t &set_max_history_size (const size_t &siz = 0);

    /**
     * @brief Resume paused playback and empty playback buffer
     *
     * @param v
     * @return std::pair<std::deque<MCTrack>, int> a list of removed track and
     *                                             a status of int 0 on
     *                                             success, > 0 on vote, -1 on
     *                                             failure
     */
    std::pair<std::deque<MCTrack>, int> skip_playback (dpp::voiceconn *v);
    std::pair<std::deque<MCTrack>, int> skip_playback (dpp::discord_voice_client *voiceclient);

    /**
     * @brief Skip track entries in the queue
     *
     * Caller should locks mutex before calling this method
     *
     * @param amount the amount of track to skip
     * @param remove force remove regardless of loop setting
     * @param pop_current force to include currently playing track
     *                    (index 0)
     *
     * @return std::deque<MCTrack> list of removed track
     */
    std::deque<MCTrack> skip_queue (int64_t amount = 1, bool remove = false, bool pop_current = false, bool push_back = false);

    /**
     * @brief Set player auto play mode
     *
     * @param state
     * @return guild_player_t&
     */
    guild_player_t &set_auto_play (const bool state = true);

    /**
     * @brief Reorganize track,
     * move currently playing track to front of the queue and reset
     * shifted_track to 0, always do this before making changes to tracks
     * position in the queue.
     *
     * @return true
     * @return false
     */
    bool reset_shifted ();

    // int64 for compatibility with command argument type
    guild_player_t &set_loop_mode (int64_t mode);

    /**
     * @brief Set player channel, used in playback infos.
     *
     * @param channel_id
     * @return guild_player_t&
     */
    guild_player_t &set_channel (const dpp::snowflake &channel_id);

    size_t remove_track (const size_t &pos, size_t amount = 1, const size_t &to = -1);

    size_t remove_track_by_user (const dpp::snowflake &user_id);

    bool pause (dpp::discord_client *from, const dpp::snowflake &user_id);

    bool shuffle (bool _update_info_embed = true);

    bool current_track_is_first_track () const;

    guild_player_t &stop ();

    // return pair of message and status 0 if has valid info_message
    std::pair<dpp::message, int> get_info_message ();
    guild_player_t &set_info_message (const dpp::message &message);

    int init_for_stream ();
    guild_player_t &done_streaming ();

    // ============================== FILTERS =============================

    // methods to check if any filter is active

    bool fx_is_tempo_active () const;
    bool fx_is_pitch_active () const;
    bool fx_is_equalizer_active () const;
    bool fx_is_sampling_rate_active () const;

    bool fx_has_vibrato_f () const;
    bool fx_has_vibrato_d () const;
    bool fx_is_vibrato_active () const;

    bool fx_has_tremolo_f () const;
    bool fx_has_tremolo_d () const;
    bool fx_is_tremolo_active () const;

    bool fx_is_earwax_active () const;

    // !TODO: methods to check if any filter should update

    // get active fx count
    int fx_get_active_count () const;

    int load_fx_states (const nlohmann::json &fx_states);

    nlohmann::json fx_states_to_json ();

    std::string get_filter_descr ();

    // ====================================================================

    void check_for_to_seek ();
    void reset_first_track_current_byte ();

    // don't need to lock mutex
    dpp::voiceconn *get_voice_conn ();
    // don't need to lock mutex
    dpp::discord_voice_client *get_voice_client ();

    // queue operations
    guild_player_t &queue_add (const MCTrack &t);
    guild_player_t &queue_pop ();
    guild_player_t &queue_add_front (const MCTrack &t);
    guild_player_t &queue_pop_front ();

    guild_player_t &queue_insert (const MCTrack &t, size_t pos);
    guild_player_t &queue_erase (size_t pos);
    track_queue::iterator queue_erase_i (track_queue::iterator i);

    guild_player_t &set_queue (const track_queue &q);
    guild_player_t &queue_clear ();
};

/////////////////////////////////////////////////////////////////////////////////////
} // namespace musicat::player

#endif // SHA_PLAYER_H

// vim: et sw=4 ts=8
