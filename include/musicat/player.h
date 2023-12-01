#ifndef SHA_PLAYER_H
#define SHA_PLAYER_H

#include "musicat/config.h"
#include "yt-search/yt-search.h"
#include "yt-search/yt-track-info.h"
#include <deque>
#include <dpp/dpp.h>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#if !defined(MUSICAT_USE_PCM)
#include <oggz/oggz.h>
#endif

namespace musicat
{
namespace player
{

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

enum processor_state_t
{
    PROCESSOR_NULL = 0,
    PROCESSOR_READY = 1,
    PROCESSOR_DEAD = (1 << 1),
};

struct MCTrack : yt_search::YTrack
{
    /**
     * @brief Downloaded track file path.
     *
     */
    std::string filename;

    /**
     * @brief User Id which added this track.
     *
     */
    dpp::snowflake user_id;

    /**
     * @brief List of user voted for this message
     *
     */
    std::deque<dpp::snowflake> skip_vote;

    yt_search::audio_info_t info;

    bool seekable;

    // seek query, reset to empty string after seek performed.
    // ffmpeg -ss value
    std::string seek_to;

    // whether this track is in the process to stop
    // its audio stream
    bool stopping;

    // current byte position
    int64_t current_byte;

    size_t filesize;

    MCTrack ();
    MCTrack (const yt_search::YTrack &t);
    ~MCTrack ();
};

struct track_progress
{
    int64_t current_ms;
    int64_t duration;
    int status;
};

class Manager;
using player_manager_ptr_t = Manager *;

class Player
{
  public:
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
    std::shared_ptr<dpp::message> info_message;

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

    /**
     * @brief Resample raw ffmpeg opt
     */
    bool set_sampling_rate;

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
    dpp::discord_client *from;
    Manager *manager;

    /**
     * @brief Track queue.
     *
     */
    std::deque<MCTrack> queue;

    /**
     * @brief Current track stream
     *
     */
    MCTrack current_track;

    int volume;
    int set_volume;

    /**
     * @brief Equalizer raw ffmpeg opt
     */
    std::string equalizer;

    /**
     * @brief Equalizer raw ffmpeg opt
     * !TODO: change this to bool
     */
    std::string set_equalizer;

    /**
     * @brief Resample raw ffmpeg opt
     */
    int64_t sampling_rate;

    bool stopped;
    bool earwax;
    bool set_earwax;

    bool set_vibrato;
    bool set_tremolo;

    bool tried_continuing;

    /**
     * @brief In percent, so it should be converted with (float)d/100 when
     * provided to ffmpeg
     */
    int vibrato_d;

    /**
     * @brief In percent, so it should be converted with (float)d/100 when
     * provided to ffmpeg
     */
    int tremolo_d;

    double vibrato_f;
    double tremolo_f;

    /**
     * @brief Thread safety mutex. Must lock this whenever doing the
     * appropriate action.
     */
    std::mutex t_mutex;

    void init ();

    Player ();
    Player (dpp::cluster *_cluster, const dpp::snowflake &_guild_id);
    ~Player ();

    Player &add_track (MCTrack &track, bool top = false,
                       const dpp::snowflake &guild_id = 0,
                       const bool update_embed = true,
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
    std::pair<std::deque<MCTrack>, int> skip (dpp::voiceconn *v);

    /**
     * @brief Skip track entries in the queue
     * @param amount the amount of track to skip
     * @param remove force remove regardless of loop setting
     * @param pop_current force to include currently playing track
     *                    (index 0)
     *
     * @return std::deque<MCTrack> list of removed track
     */
    std::deque<MCTrack> skip_queue (int64_t amount = 1,
                                    const bool remove = false,
                                    const bool pop_current = false);

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

    size_t remove_track (const size_t &pos, size_t amount = 1,
                         const size_t &to = -1);

    size_t remove_track_by_user (const dpp::snowflake &user_id);

    bool pause (dpp::discord_client *from,
                const dpp::snowflake &user_id) const;

    bool shuffle ();

    void set_stopped (const bool &val);

    bool is_stopped () const;
};

class Manager
{
  public:
    dpp::cluster *cluster;
    std::map<dpp::snowflake, std::shared_ptr<Player> > players;
    std::map<dpp::snowflake, std::shared_ptr<dpp::message> >
        info_messages_cache;

    // Mutexes
    // dl: waiting_file_download
    // wd: waiting_vc_ready
    // c: connecting
    // dc: disconnecting
    // ps: players
    // mp: manually_paused
    // imc: info_messages_cache
    // im: ignore_marker
    // sq: stop_queue
    // as: audio_stream
    std::mutex dl_m, wd_m, c_m, dc_m, ps_m, mp_m, imc_m, im_m, sq_m, as_m;

    // Conditional variable, use notify_all
    std::condition_variable dl_cv, stop_queue_cv, as_cv;
    std::map<dpp::snowflake, dpp::snowflake> connecting, disconnecting;
    std::map<dpp::snowflake, std::string> waiting_vc_ready;
    std::map<std::string, dpp::snowflake> waiting_file_download;
    std::map<std::string, processor_state_t> processor_states;
    std::map<dpp::snowflake, std::vector<std::string> > waiting_marker;
    std::vector<dpp::snowflake> manually_paused;
    std::map<dpp::snowflake, bool> stop_queue;
    std::vector<dpp::snowflake> ignore_marker;

    Manager (dpp::cluster *_cluster);
    ~Manager ();

    /**
     * @brief Create a player object if not exist and return player
     *
     * @param guild_id
     * @return std::shared_ptr<Player>
     */
    std::shared_ptr<Player> create_player (const dpp::snowflake &guild_id);

    /**
     * @brief Get the player object, return NULL if not exist
     *
     * @param guild_id
     * @return std::shared_ptr<Player>
     */
    std::shared_ptr<Player> get_player (const dpp::snowflake &guild_id);

    void reconnect (dpp::discord_client *from, const dpp::snowflake &guild_id);

    /**
     * @brief Return false if guild doesn't have player in the first place
     *
     * @param guild_id
     * @return true
     * @return false
     */
    bool delete_player (const dpp::snowflake &guild_id);

    /**
     * @brief Get guild player's queue, return NULL if player not exist
     *
     * @param guild_id
     * @return std::deque<MCTrack>
     */
    std::deque<MCTrack> get_queue (const dpp::snowflake &guild_id);

    /**
     * @brief Manually pause guild player
     *
     * @param from
     * @param guild_id
     * @param user_id
     * @return true
     * @return false
     * @throw musicat::exception
     */
    bool pause (dpp::discord_client *from, const dpp::snowflake &guild_id,
                const dpp::snowflake &user_id);

    void unpause (dpp::discord_voice_client *voiceclient,
                  const dpp::snowflake &guild_id);

    bool is_disconnecting (const dpp::snowflake &guild_id);

    void set_disconnecting (const dpp::snowflake &guild_id,
                            const dpp::snowflake &voice_channel_id);

    void clear_disconnecting (const dpp::snowflake &guild_id);

    bool is_connecting (const dpp::snowflake &guild_id);

    void set_connecting (const dpp::snowflake &guild_id,
                         const dpp::snowflake &voice_channel_id);

    int clear_connecting (const dpp::snowflake &guild_id);

    bool is_waiting_vc_ready (const dpp::snowflake &guild_id);

    void set_waiting_vc_ready (const dpp::snowflake &guild_id,
                               const std::string &second = "0");

    void set_vc_ready_timeout (const dpp::snowflake &guild_id,
                               const unsigned long &timer = 10000);

    void wait_for_vc_ready (const dpp::snowflake &guild_id);

    int clear_wait_vc_ready (const dpp::snowflake &guild_id);

    bool is_manually_paused (const dpp::snowflake &guild_id);

    void set_manually_paused (const dpp::snowflake &guild_id);

    void clear_manually_paused (const dpp::snowflake &guild_id);

    void set_processor_state (std::string &server_id_str,
                              processor_state_t state);

    processor_state_t get_processor_state (std::string &server_id_str);

    bool is_processor_ready (std::string &server_id_str);

    bool is_processor_dead (std::string &server_id_str);

    /**
     * @brief Check whether client is ready to stream in vc and make changes to
     * playback and player queue, will auto reconnect if `from` provided
     *
     * @param guild_id
     * @param from
     * @param user_id User who's invoked the function and in a vc
     * @return true
     * @return false
     */
    bool voice_ready (const dpp::snowflake &guild_id,
                      dpp::discord_client *from = nullptr,
                      const dpp::snowflake &user_id = 0);

    /**
     * @brief Stop guild player audio stream
     */
    void stop_stream (const dpp::snowflake &guild_id);

    /**
     * @brief Skip currently playing song
     *
     * @param v
     * @param guild_id
     * @param user_id
     * @param remove
     *
     * @return std::pair<std::deque<MCTrack>, int> a list of removed track and
     *                                             a status of int 0 on
     *                                             success, > 0 on vote, -1 on
     *                                             failure
     *
     * @throw musicat::exception
     */
    std::pair<std::deque<MCTrack>, int> skip (dpp::voiceconn *v,
                                              const dpp::snowflake &guild_id,
                                              const dpp::snowflake &user_id,
                                              const int64_t &amount = 1,
                                              const bool remove = false);

    void download (const std::string &fname, const std::string &url,
                   const dpp::snowflake &guild_id);

    void wait_for_download (const std::string &file_name);

    bool is_waiting_file_download (const std::string &file_name);

    void stream (dpp::discord_voice_client *v, player::MCTrack &track);
    bool is_stream_stopping (const dpp::snowflake &guild_id);
    int set_stream_stopping (const dpp::snowflake &guild_id);
    int clear_stream_stopping (const dpp::snowflake &guild_id);

    void prepare_play_stage_channel_routine (
        dpp::discord_voice_client *voice_client, dpp::guild *guild);

    /**
     * @brief Start streaming thread, plays `track` on `v`
     * @param v voice client to stream on
     * @param track track to play
     * @param channel_id text channel for sending nowplaying embed
     * @return int 0 on success, 1 on fail
     */
    int play (dpp::discord_voice_client *v, player::MCTrack &track,
              const dpp::snowflake &channel_id = 0);

    /**
     * @brief Try to send currently playing song info to player channel
     *
     * @param guild_id
     * @param update Whether to update last info embed instead of sending new
     * one, return false if no info embed exist
     * @param force_playing_status
     * @param event event to reply/update to
     * @return true
     * @return false
     * @throw musicat::exception
     */
    bool send_info_embed (const dpp::snowflake &guild_id, bool update = false,
                          const bool force_playing_status = false,
                          const dpp::interaction_create_t *event = nullptr);

    /**
     * @brief Update currently playing song info embed, return false if no info
     * embed exist
     *
     * @param guild_id
     * @param force_playing_status
     * @param event event to reply/update to
     * @return true
     * @return false
     * @throw musicat::exception
     */
    bool update_info_embed (const dpp::snowflake &guild_id,
                            const bool force_playing_status = false,
                            const dpp::interaction_create_t *event = nullptr);

    /**
     * @brief Delete currently playing song info embed, return false if no info
     * embed exist
     *
     * @param guild_id
     * @param callback Function to call after message deleted
     * @return true - Message is deleted
     * @return false - No player or no info embed exist
     */
    bool delete_info_embed (const dpp::snowflake &guild_id,
                            dpp::command_completion_event_t callback
                            = dpp::utility::log_error ());

    /**
     * @brief Get all available track to use
     * @param amount Amount of track to return
     *
     * @return std::vector<std::string>
     */
    std::vector<std::string> get_available_tracks (const size_t &amount
                                                   = 0) const;

    bool handle_on_track_marker (const dpp::voice_track_marker_t &event);

    dpp::embed get_playing_info_embed (const dpp::snowflake &guild_id,
                                       const bool force_playing_status);

    void handle_on_voice_ready (const dpp::voice_ready_t &event);
    void handle_on_voice_state_update (const dpp::voice_state_update_t &event);

    bool set_info_message_as_deleted (dpp::snowflake id);
    void handle_on_message_delete (const dpp::message_delete_t &event);
    void
    handle_on_message_delete_bulk (const dpp::message_delete_bulk_t &event);

    /**
     * @brief Remove guild's queue's amount of track starting from pos
     *
     * @param guild_id
     * @param pos
     * @param amount
     * @param to
     * @return size_t Amount of track actually removed
     */
    size_t remove_track (const dpp::snowflake &guild_id, const size_t &pos,
                         const size_t &amount = 1, const size_t &to = -1);

    bool shuffle_queue (const dpp::snowflake &guild_id);

    void set_ignore_marker (const dpp::snowflake &guild_id);
    void remove_ignore_marker (const dpp::snowflake &guild_id);
    bool has_ignore_marker (const dpp::snowflake &guild_id);

    /**
     * @brief Load guild queue saved in database, you wanna call this after the
     * bot rebooted.
     *
     * @param guild_id
     * @param user_id User to be the author of the tracks added
     * @return int -1 if json is null, -2 if no row found in the table else 0
     */
    int load_guild_current_queue (const dpp::snowflake &guild_id,
                                  const dpp::snowflake *user_id = NULL);

    /**
     * @brief Load guild player config saved in database, you wanna call this
     * after the bot rebooted.
     *
     * @param guild_id
     * @return int 1 if everything is default value, 0 if something loaded.
     */
    int load_guild_player_config (const dpp::snowflake &guild_id);

    /**
     * @brief Prepare for reconnect by setting necessary state,
     *        must call dpp::discord_client::disconnect_voice (dpp::snowflake
     * guild_id) after calling this method, and then create a thread to call
     *        player_manager::reconnect (dpp::discord_client *from,
     * dpp::snowflake guild_id) for the reconnect to succeed
     *
     * @return int 0 if success, -1 guild_id is 0, 1 connect_channel_id is 0
     */
    int set_reconnect (const dpp::snowflake &guild_id,
                       const dpp::snowflake &disconnect_channel_id,
                       const dpp::snowflake &connect_channel_id);

    /**
     * @brief Perform full voice channel reconnect/rejoin
     */
    int full_reconnect (dpp::discord_client *from,
                        const dpp::snowflake &guild_id,
                        const dpp::snowflake &disconnect_channel_id,
                        const dpp::snowflake &connect_channel_id,
                        const bool &for_listener = false);

    /**
     * @brief Fetch next autoplay track and add it to queue
     */
    void get_next_autoplay_track (const std::string &track_id,
                                  dpp::discord_client *from,
                                  const dpp::snowflake &server_id);
};

/////////////////////////////////////////////////////////////////////////////////////

struct handle_effect_chain_change_states_t
{
    std::shared_ptr<Player> &guild_player;
    player::MCTrack &track;
    int &command_fd;
    int &read_fd;
    void /*OGGZ*/ *track_og;
    dpp::discord_voice_client *vc;
    int notification_fd;
};

using effect_states_list_t
    = std::vector<handle_effect_chain_change_states_t *>;

extern std::mutex effect_states_list_m; // EXTERN_VARIABLE

/**
 * @brief Get guild effect states.
 * Must lock `effect_states_list_m` until done using the returned ptr
 */
handle_effect_chain_change_states_t *
get_effect_states (const dpp::snowflake &guild_id);

/**
 * @brief Get effect states list.
 * Must lock `effect_states_list_m` until done using the returned ptr
 */
effect_states_list_t *get_effect_states_list ();

} // player

/////////////////////////////////////////////////////////////////////////////////////

namespace util
{

/**
 * @brief Check if guild player has current track loaded
 */
bool player_has_current_track (std::shared_ptr<player::Player> guild_player);

/**
 * @brief Get track current progress in ms
 */
player::track_progress get_track_progress (player::MCTrack &track);

} // util
} // musicat

#endif // SHA_PLAYER_H

// vim: et sw=4 ts=8
