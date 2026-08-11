#ifndef SHA_PLAYER_MANAGER_H
#define SHA_PLAYER_MANAGER_H

#include <cstddef>
#include <dpp/dpp.h>
#include <map>

namespace musicat::player
{

#ifndef MUSICAT_MCTRACK_H
#warning Missing #include "musicat/mctrack.h"
struct MCTrack;
#endif // MUSICAT_MCTRACK_H

#ifndef SHA_PLAYER_H
#warning Missing #include "musicat/player.h"
class guild_player_t
{
  public:
    static const uint32_t INVALID_SHARD_ID;
};
#endif // SHA_PLAYER_H

struct get_playing_info_embed_info_t
{
    const char *play_pause_icon;
    bool playing;
    bool notification;
    bool stopped;

    get_playing_info_embed_info_t () : play_pause_icon (NULL), playing (false), notification (true), stopped (false) {}

    ~get_playing_info_embed_info_t () = default;
};

namespace manager
{

void handle_guild_delete (const dpp::guild_delete_t &e);

void set_shutdown_skip_close_voice_sessions (bool state);

std::lock_guard<std::mutex> acquire_players ();

std::map<dpp::snowflake, std::shared_ptr<guild_player_t> > *get_players ();

/**
 * @brief Create a player object if not exist and return player
 *
 * @param guild_id
 * @return std::shared_ptr<guild_player_t>
 */
std::shared_ptr<guild_player_t> create_player (const dpp::snowflake &guild_id);

/**
 * @brief Get the player object, return NULL if not exist
 *
 * @param guild_id
 * @return std::shared_ptr<guild_player_t>
 */
std::shared_ptr<guild_player_t> get_player (const dpp::snowflake &guild_id);

/**
 * @brief Get discord_client object, return NULL if not exist
 *
 * @param shard_id
 * @return dpp::discord_client*
 */
dpp::discord_client *get_client (uint32_t shard_id);

// shutdown Manager
void shutdown ();

// waits for disconnect then request to connect and waits for connected event
void reconnect (dpp::discord_client *from, const dpp::snowflake &guild_id);
// waits for disconnect then request to connect and waits for connected event
void reconnect (const uint32_t shard_id, const dpp::snowflake &guild_id);

// check connection health, auto reconnect if needed
void check_health (const dpp::snowflake &guild_id);

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
 * @param guild_id
 * @param user_id
 * @return true
 * @return false
 * @throw musicat::exception
 */
bool pause (const dpp::snowflake &guild_id, const dpp::snowflake &user_id, bool _update_info_embed = true);
void unpause (dpp::discord_voice_client *voiceclient, const dpp::snowflake &guild_id, bool _update_info_embed = true);

bool is_disconnecting (const dpp::snowflake &guild_id);
void set_disconnecting (const dpp::snowflake &guild_id, const dpp::snowflake &voice_channel_id);

// overrides dpp::discord_client::disconnect_voice()
void disconnect_voice (dpp::discord_client *dc, const dpp::snowflake &guild_id, bool force = false);

/**
 * @brief Returns 1 if doesn't need to wait, 0 otherwise
 */
int wait_for_disconnecting (const dpp::snowflake &guild_id);
void clear_disconnecting (const dpp::snowflake &guild_id);

bool is_connecting (const dpp::snowflake &guild_id);
void set_connecting (const dpp::snowflake &guild_id, const dpp::snowflake &voice_channel_id);
int clear_connecting (const dpp::snowflake &guild_id);

bool is_waiting_vc_ready (const dpp::snowflake &guild_id);
void set_waiting_vc_ready (const dpp::snowflake &guild_id, const std::string &second = "0");
void set_vc_ready_timeout (const dpp::snowflake &guild_id, /* in second */ const unsigned long &timer = 10);

/**
 * @brief Returns 1 if doesn't need to wait, 0 otherwise
 */
int wait_for_vc_ready (const dpp::snowflake &guild_id);
int clear_wait_vc_ready (const dpp::snowflake &guild_id);

bool is_manually_paused (const dpp::snowflake &guild_id);
void set_manually_paused (const dpp::snowflake &guild_id);
void clear_manually_paused (const dpp::snowflake &guild_id);

/**
 * @brief Check whether client is ready to stream in vc and make changes to
 * playback and player queue, will auto reconnect if `from` provided
 *
 * @param guild_id
 * @param shard_id
 * @param user_id User who's invoked the function and in a vc
 * @return true
 * @return false
 */
bool voice_ready (const dpp::snowflake &guild_id, const uint32_t shard_id = guild_player_t::INVALID_SHARD_ID,
                  const dpp::snowflake &user_id = 0);

/**
 * @brief Stop guild player audio stream
 */
int stop_stream (const dpp::snowflake &guild_id);

/**
 * @brief Skip currently playing song
 * Will lock guild_player until returned
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
std::pair<std::deque<MCTrack>, int> skip (dpp::voiceconn *v, const dpp::snowflake &guild_id, const dpp::snowflake &user_id,
                                          const int64_t &amount = 1, const bool remove = false);

void download (const std::string &fname, const std::string &url, const dpp::snowflake &guild_id);
void wait_for_download (const std::string &file_name);
bool is_waiting_file_download (const std::string &file_name);

void submit_stream_ctx (const dpp::snowflake &guild_id);

void prepare_play_stage_channel_routine (dpp::discord_voice_client *voice_client, dpp::guild *guild);

/**
 * @brief Start streaming thread
 * @param guild_id guild Id to start stream on
 * @return int 0 on success, 1 on fail
 */
int play (const dpp::snowflake &guild_id);

/**
 * @brief Try to send currently playing song info to player channel
 *
 * @param guild_id
 * @param update Whether to update last info embed instead of sending new
 * one, return false if no info embed exist
 * @param force_playing_status
 * @param event event to reply/update to
 * @returns 0 on success, 1 no player, 2 no permission
 */
int send_info_embed (const dpp::snowflake &guild_id, bool update = false, const bool force_playing_status = false,
                     const dpp::interaction_create_t *event = nullptr);

/**
 * @brief Update currently playing song info embed, return false if no info
 * embed exist
 *
 * @param guild_id
 * @param force_playing_status
 * @param event event to reply/update to
 * @returns 0 on success, 1 no player, 2 no permission
 */
int update_info_embed (const dpp::snowflake &guild_id, const bool force_playing_status = false,
                       const dpp::interaction_create_t *event = nullptr);

/**
 * @brief For use in slash command
 */
void reply_info_embed (const dpp::interaction_create_t &event, bool expand_button, bool reply_update_message = false);

/**
 * @brief Delete currently playing song info embed, return false if no info
 * embed exist
 *
 * @param guild_id
 * @param callback Function to call after message deleted
 * @return true - Message is deleted
 * @return false - No player or no info embed exist
 */
bool delete_info_embed (const dpp::snowflake &guild_id, const dpp::command_completion_event_t &callback = dpp::utility::log_error ());

bool handle_on_track_marker (const dpp::voice_track_marker_t &event);

void spawn_handle_track_marker_worker (const dpp::voice_track_marker_t &event);

/**
 * @brief Returns embed and status.
 *
 * Statuses:
 * 0: success.
 * 1: No player.
 * 2: No track.
 *
 * -1: other error (dpp/logic error).
 */
std::pair<dpp::embed, int> get_playing_info_embed (const dpp::snowflake &guild_id, bool force_playing_status,
                                                   get_playing_info_embed_info_t *info_struct = NULL);

/**
 * @brief Get complete playing info message, returns status code.
 * Statuses:
 * get_playing_info_embed() statuses.
 */
int get_playing_info_message (dpp::message &msg, const dpp::snowflake &guild_id, bool force_playing_status, bool button_expanded);

void handle_on_voice_ready (const dpp::voice_ready_t &event);

void handle_non_sha_voice_state_update (const dpp::voice_state_update_t &event);
void handle_sha_voice_state_update (const dpp::voice_state_update_t &event);

void handle_on_voice_state_update (const dpp::voice_state_update_t &event);

bool set_info_message_as_deleted (dpp::snowflake guild_id, dpp::snowflake message_id);
void handle_on_message_delete (const dpp::message_delete_t &event);
void handle_on_message_delete_bulk (const dpp::message_delete_bulk_t &event);

/**
 * @brief Remove guild's queue's amount of track starting from pos
 *
 * @param guild_id
 * @param pos
 * @param amount
 * @param to
 * @return size_t Amount of track actually removed
 */
size_t remove_track (const dpp::snowflake &guild_id, const size_t &pos, const size_t &amount = 1, const size_t &to = -1);

bool shuffle_queue (const dpp::snowflake &guild_id, bool _update_info_embed = true);

/**
 * @brief Load guild queue saved in database, you wanna call this after the
 * bot rebooted.
 *
 * @param guild_id
 * @param user_id User to be the author of the tracks added
 * @return int -1 if json is null, -2 if no row found in the table else 0
 */
int load_guild_current_queue (const dpp::snowflake &guild_id, const dpp::snowflake *user_id = NULL);

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
 *        must call disconnect_voice (dpp::discord_client *dc, const dpp::snowflake &guild_id) after calling this method, and then
 * create a thread to call player_manager::reconnect (dpp::discord_client *from, dpp::snowflake guild_id) for the reconnect to succeed
 *
 * @return int 0 if success, -1 guild_id is 0, 1 connect_channel_id is 0
 */
int set_reconnect (const dpp::snowflake &guild_id, const dpp::snowflake &disconnect_channel_id, const dpp::snowflake &connect_channel_id);

/**
 * @brief Perform full voice channel reconnect/rejoin
 */
int full_reconnect (dpp::discord_client *from, const dpp::snowflake &guild_id, const dpp::snowflake &disconnect_channel_id,
                    const dpp::snowflake &connect_channel_id, const bool &for_listener = false);

/**
 * @brief Fetch next autoplay track and add it to queue
 */
void get_next_autoplay_track (const std::string &track_id, const uint32_t shard_id, const dpp::snowflake &server_id);

/**
 * @brief Set autopause if needed
 * returns 0 if autopause set, 1 if bad arg, -1 if not set
 */
int set_autopause (dpp::voiceconn *v, const dpp::snowflake &guild_id, bool check_listening_user = true);

void check_autopause (const dpp::snowflake &e_guild_id, const dpp::snowflake &e_voice_channel_id);

} // namespace manager

} // namespace musicat::player

#endif // SHA_PLAYER_MANAGER_H
