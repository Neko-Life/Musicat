#ifndef SHA_PLAYER_H
#define SHA_PLAYER_H

#include "mctrack.h"
#include "musicat/audio_config.h"

#ifdef USING_LIBOPUSENC
#include "opusenc.h"
#else
#include "opus/opus.h"
#endif // USING_LIBOPUSENC

#include <dpp/dpp.h>
#include <cstdint>
#include <deque>
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

class Manager;

class Player
{
  public:
    using track_queue = std::deque<MCTrack>;

    static const uint32_t INVALID_SHARD_ID = 0xFFFFFFFF;

    /**
     * @brief Guild Id this player belongs to.
     *
     */
    dpp::snowflake guild_id;

    /**
     * @brief Latest channel Id where the command invoked to play/add song.
     *
     */
    dpp::snowflake channel_id;

    /**
     * @brief Message info of currently playing song.
     *
     */
    nlohmann::json info_message;

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

    dpp::cluster *cluster;
    Manager *manager;

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

    uint32_t shard_id;

    // default 100
    int volume;

    /**
     * @brief Equalizer raw ffmpeg opt
     */
    std::string equalizer;

    /**
     * @brief Default -1
     */
    int64_t sampling_rate;

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

    bool stopped;
    bool earwax;

    bool tried_continuing;

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

#ifdef USING_LIBOPUSENC
    OggOpusEnc *opus_encoder;
    OggOpusComments *opus_encoder_comments;
#else
    OpusEncoder *opus_encoder;
#endif

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
     * @brief Thread safety mutex. Must lock this whenever doing the
     * appropriate action.
     */
    std::mutex t_mutex, stream_m;

    void init ();

    Player ();
    Player (dpp::cluster *_cluster, const dpp::snowflake &_guild_id);
    ~Player ();

    dpp::discord_client *get_client ();

    void set_shard (dpp::discord_client *from);

    void set_shard (uint32_t shard_id);

    // see if adding this track will block
    static bool add_track_will_block (const MCTrack &track);

    Player &add_track (MCTrack &track, bool top = false, const dpp::snowflake &guild_id = 0, const bool update_embed = true,
                       const int64_t &arg_slip = 0);

    Player &set_max_history_size (const size_t &siz = 0);

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
     * Caller should locks t_mutex before calling this method
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
     * @return Player&
     */
    Player &set_auto_play (const bool state = true);

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
    Player &set_loop_mode (int64_t mode);

    /**
     * @brief Set player channel, used in playback infos.
     *
     * @param channel_id
     * @return Player&
     */
    Player &set_channel (const dpp::snowflake &channel_id);

    size_t remove_track (const size_t &pos, size_t amount = 1, const size_t &to = -1);

    size_t remove_track_by_user (const dpp::snowflake &user_id);

    bool pause (dpp::discord_client *from, const dpp::snowflake &user_id);

    bool shuffle (bool update_info_embed = true);

    bool current_track_is_first_track () const;

    Player &stop ();

    // return pair of message and status 0 if has valid info_message
    std::pair<dpp::message, int> get_info_message ();
    Player &set_info_message (const dpp::message &message);

    int init_for_stream ();
    Player &done_streaming ();

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
    dpp::voiceconn *get_voice_conn ();
    dpp::discord_voice_client *get_voice_client ();

    // queue operations
    Player &queue_add (const MCTrack &t);
    Player &queue_pop ();
    Player &queue_add_front (const MCTrack &t);
    Player &queue_pop_front ();

    Player &queue_insert (const MCTrack &t, size_t pos);
    Player &queue_erase (size_t pos);
    track_queue::iterator queue_erase_i (track_queue::iterator i);

    Player &set_queue (const track_queue &q);
    Player &queue_clear ();
};

/////////////////////////////////////////////////////////////////////////////////////
} // namespace musicat::player

#endif // SHA_PLAYER_H

// vim: et sw=4 ts=8
