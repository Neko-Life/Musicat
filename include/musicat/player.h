#ifndef SHA_PLAYER_H
#define SHA_PLAYER_H

#include <deque>
#include <dpp/dpp.h>
#include "yt-search.h"
#include "nlohmann/json.hpp"
#include "musicat/musicat.h"

namespace mc = musicat;
using string = std::string;

// TODO: Player class

struct Sha_Track : YTrack {
    /**
     * @brief Downloaded track name.
     *
     */
    string filename;

    /**
     * @brief User Id which added this track.
     *
     */
    dpp::snowflake user_id;

    Sha_Track();
    Sha_Track(YTrack t);
    ~Sha_Track();
};

class Sha_Player {
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
    dpp::message* info_message;

    /**
     * @brief Loop mode of the currently playing song.
     *
     * 0: Not looping,
     * 1: Looping one song and remove skipped song,
     * 2: Looping queue,
     * 3: Looping one song and won't remove skipped song.
     *
     */
    short int loop_mode;

    /**
     * @brief Number of added track to the front of queue.
     *
     */
    size_t shifted_track;

    dpp::cluster* cluster;

    /**
     * @brief Track queue.
     *
     */
    std::deque<Sha_Track>* queue;

    /**
     * @brief Must use this whenever doing the appropriate action.
     *
     * q: queue,
     * ch: channel_id,
     * st: shifted_track.
     *
     */
    std::mutex skip_mutex, st_m, q_m, ch_m;

    Sha_Player(dpp::cluster* _cluster, dpp::snowflake _guild_id);
    ~Sha_Player();
    Sha_Player& add_track(Sha_Track track, bool top = false);
    bool skip(dpp::voiceconn* v, dpp::snowflake user_id, int amount = 1);

    /**
     * @brief Reorganize track,
     * move currently playing track to front of the queue and reset shifted_track to 0,
     * always do this before making changes to tracks position in the queue.
     *
     * @return true
     * @return false
     */
    bool reset_shifted();

    Sha_Player& set_loop_mode(short int mode);

    /**
     * @brief Set player channel, used in playback infos.
     *
     * @param channel_id
     * @return Sha_Player&
     */
    Sha_Player& set_channel(dpp::snowflake channel_id);
    int remove_track(int pos, int amount = 1);
    int remove_track_by_user(dpp::snowflake user_id, int amount = -1);
    bool pause(dpp::discord_client* from, dpp::snowflake user_id);
    int seek(int pos, bool abs);
    int stop();
    int resume();
    int search(string query);
    int join();
    int leave();
    int rejoin();
};

class Sha_Player_Manager {
public:
    dpp::cluster* cluster;
    std::map<dpp::snowflake, Sha_Player*>* players;
    dpp::snowflake sha_id;

    // Mutexes
    // dl: waiting_file_download
    // wd: waiting_vc_ready
    // c: connecting
    // dc: disconnecting
    // ps: players
    // mp: manually_paused
    // sq: stop_queue
    std::mutex dl_m, wd_m, c_m, dc_m, ps_m, mp_m, sq_m;

    // Conditional variable, use notify_all
    std::condition_variable dl_cv, stop_queue_cv;
    std::map<uint64_t, uint64_t> connecting, disconnecting;
    std::map<uint64_t, string> waiting_vc_ready;
    std::map<string, uint64_t> waiting_file_download;
    std::map<uint64_t, std::vector<string>> waiting_marker;
    std::vector<uint64_t> manually_paused;
    std::vector<dpp::snowflake> stop_queue;

    Sha_Player_Manager(dpp::cluster* _cluster, dpp::snowflake _sha_id);
    ~Sha_Player_Manager();

    /**
     * @brief Create a player object if not exist and return player
     *
     * @param guild_id
     * @return Sha_Player*
     */
    Sha_Player* create_player(dpp::snowflake guild_id);

    /**
     * @brief Get the player object, return NULL if not exist
     *
     * @param guild_id
     * @return Sha_Player*
     */
    Sha_Player* get_player(dpp::snowflake guild_id);
    void reconnect(dpp::discord_client* from, dpp::snowflake guild_id);

    /**
     * @brief Return false if guild doesn't have player in the first place
     *
     * @param guild_id
     * @return true
     * @return false
     */
    bool delete_player(dpp::snowflake guild_id);

    /**
     * @brief Get guild player's queue, return NULL if player not exist
     *
     * @param guild_id
     * @return std::deque<Sha_Track>*
     */
    std::deque<Sha_Track>* get_queue(dpp::snowflake guild_id);

    /**
     * @brief Manually pause guild player
     *
     * @param from
     * @param guild_id
     * @param user_id
     * @return true
     * @return false
     * @throw mc::exception
     */
    bool pause(dpp::discord_client* from, dpp::snowflake guild_id, dpp::snowflake user_id);
    bool unpause(dpp::discord_voice_client* voiceclient, dpp::snowflake guild_id);

    /**
     * @brief Check whether client is ready to stream in vc and make changes to playback and player queue
     *
     * @param guild_id
     * @return true
     * @return false
     */
    bool voice_ready(dpp::snowflake guild_id);
    void stop_stream(dpp::snowflake guild_id);

    /**
     * @brief Skip currently playing song
     *
     * @param v
     * @param guild_id
     * @param user_id
     * @return true
     * @return false
     * @throw mc::exception
     */
    bool skip(dpp::voiceconn* v, dpp::snowflake guild_id, dpp::snowflake user_id);
    void download(string fname, string url, dpp::snowflake guild_id);
    void wait_for_download(string file_name);
    void stream(dpp::discord_voice_client* v, string fname, dpp::snowflake channel_id = 0);
    void play(dpp::discord_voice_client* v, string fname, dpp::snowflake channel_id = 0);

    /**
     * @brief Try to send currently playing song info to player channel
     *
     * @param guild_id
     * @param update Whether to update last info embed instead of sending new one, return false if no info embed exist
     * @return true
     * @return false
     * @throw mc::exception
     */
    bool send_info_embed(dpp::snowflake guild_id, bool update = false);

    /**
     * @brief Update currently playing song info embed, return false if no info embed exist
     *
     * @param guild_id
     * @return true
     * @return false
     * @throw mc::exception
     */
    bool update_info_embed(dpp::snowflake guild_id);

    /**
     * @brief Delete currently playing song info embed, return false if no info embed exist
     *
     * @param guild_id
     * @param callback Function to call after message deleted
     * @return true
     * @return false
     */
    bool delete_info_embed(dpp::snowflake guild_id, dpp::command_completion_event_t callback = dpp::utility::log_error());

    bool handle_on_track_marker(const dpp::voice_track_marker_t& event);
    dpp::embed get_playing_info_embed(dpp::snowflake guild_id);
    void handle_on_voice_ready(const dpp::voice_ready_t& event);
    void handle_on_voice_state_update(const dpp::voice_state_update_t& event);
};

#endif