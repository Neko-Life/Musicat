#ifndef SHA_PLAYER_H
#define SHA_PLAYER_H

#include <string>
#include <map>
#include <mutex>
#include <vector>
#include <deque>
#include <dpp/dpp.h>
#include "yt-search.h"
#include "nlohmann/json.hpp"
#include "musicat/musicat.h"

namespace musicat_player {
    namespace mc = musicat;
    using string = std::string;

    // TODO: Player class

    enum loop_mode_t : int8_t {
        // No looping
        l_none,
        // Loop song
        l_song,
        // Loop queue
        l_queue,
        // Loop song and queue
        l_song_queue
    };

    struct MCTrack : YTrack {
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

        MCTrack();
        MCTrack(YTrack t);
        ~MCTrack();
    };

    class Manager;

    class Player {
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
         * @brief Number of added track to the front of queue.
         *
         */
        size_t shifted_track;

        dpp::cluster* cluster;
        dpp::discord_client* from;
        Manager* manager;

        /**
         * @brief Track queue.
         *
         */
        std::deque<MCTrack> queue;

        /**
         * @brief Must use this whenever doing the appropriate action.
         *
         * q: queue,
         * ch: channel_id,
         * st: shifted_track.
         *
         */
        std::mutex skip_mutex, st_m, q_m, ch_m;

        Player(dpp::cluster* _cluster, dpp::snowflake _guild_id);
        ~Player();
        Player& add_track(MCTrack track, bool top = false, dpp::snowflake guild_id = 0);
        bool skip(dpp::voiceconn* v, size_t amount = 1) const;

        /**
         * @brief Reorganize track,
         * move currently playing track to front of the queue and reset shifted_track to 0,
         * always do this before making changes to tracks position in the queue.
         *
         * @return true
         * @return false
         */
        bool reset_shifted();

        Player& set_loop_mode(int8_t mode);

        /**
         * @brief Set player channel, used in playback infos.
         *
         * @param channel_id
         * @return Player&
         */
        Player& set_channel(dpp::snowflake channel_id);
        int remove_track(int pos, int amount = 1);
        int remove_track_by_user(dpp::snowflake user_id, int amount = -1);
        bool pause(dpp::discord_client* from, dpp::snowflake user_id) const;
        int seek(int pos, bool abs);
        int stop();
        int resume();
        int search(string query);
        int join();
        int leave();
        int rejoin();
    };

    class Manager {
    public:
        dpp::cluster* cluster;
        std::map<dpp::snowflake, std::shared_ptr<Player>> players;
        std::map<dpp::snowflake, std::shared_ptr<dpp::message>> info_messages_cache;
        dpp::snowflake sha_id;

        // Mutexes
        // dl: waiting_file_download
        // wd: waiting_vc_ready
        // c: connecting
        // dc: disconnecting
        // ps: players
        // mp: manually_paused
        // sq: stop_queue
        // imc: info_messages_cache
        std::mutex dl_m, wd_m, c_m, dc_m, ps_m, mp_m, sq_m, imc_m;

        // Conditional variable, use notify_all
        std::condition_variable dl_cv, stop_queue_cv;
        std::map<uint64_t, uint64_t> connecting, disconnecting;
        std::map<uint64_t, string> waiting_vc_ready;
        std::map<string, uint64_t> waiting_file_download;
        std::map<uint64_t, std::vector<string>> waiting_marker;
        std::vector<uint64_t> manually_paused;
        std::vector<dpp::snowflake> stop_queue;

        Manager(dpp::cluster* _cluster, dpp::snowflake _sha_id);
        ~Manager();

        /**
         * @brief Create a player object if not exist and return player
         *
         * @param guild_id
         * @return std::shared_ptr<Player>
         */
        std::shared_ptr<Player> create_player(dpp::snowflake guild_id);

        /**
         * @brief Get the player object, return NULL if not exist
         *
         * @param guild_id
         * @return std::shared_ptr<Player>
         */
        std::shared_ptr<Player> get_player(dpp::snowflake guild_id);
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
         * @return std::deque<MCTrack>
         */
        std::deque<MCTrack> get_queue(dpp::snowflake guild_id);

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

        bool is_disconnecting(dpp::snowflake guild_id);
        bool is_connecting(dpp::snowflake guild_id);
        bool is_waiting_vc_ready(dpp::snowflake guild_id);
        /**
         * @brief Check whether client is ready to stream in vc and make changes to playback and player queue, will auto reconnect if `from` provide
         *
         * @param guild_id
         * @param from
         * @return true
         * @return false
         */
        bool voice_ready(dpp::snowflake guild_id, dpp::discord_client* from = nullptr);
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
        void play(dpp::discord_voice_client* v, string fname, dpp::snowflake channel_id = 0, bool notify_error = false);

        /**
         * @brief Try to send currently playing song info to player channel
         *
         * @param guild_id
         * @param update Whether to update last info embed instead of sending new one, return false if no info embed exist
         * @param force_playing_status
         * @return true
         * @return false
         * @throw mc::exception
         */
        bool send_info_embed(dpp::snowflake guild_id, bool update = false, bool force_playing_status = false);

        /**
         * @brief Update currently playing song info embed, return false if no info embed exist
         *
         * @param guild_id
         * @param force_playing_status
         * @return true
         * @return false
         * @throw mc::exception
         */
        bool update_info_embed(dpp::snowflake guild_id, bool force_playing_status = false);

        /**
         * @brief Delete currently playing song info embed, return false if no info embed exist
         *
         * @param guild_id
         * @param callback Function to call after message deleted
         * @return true - Message is deleted
         * @return false - No player or no info embed exist
         */
        bool delete_info_embed(dpp::snowflake guild_id, dpp::command_completion_event_t callback = dpp::utility::log_error());

        bool handle_on_track_marker(const dpp::voice_track_marker_t& event);
        dpp::embed get_playing_info_embed(dpp::snowflake guild_id, bool force_playing_status);
        void handle_on_voice_ready(const dpp::voice_ready_t& event);
        void handle_on_voice_state_update(const dpp::voice_state_update_t& event);
        bool set_info_message_as_deleted(dpp::snowflake id);
        void handle_on_message_delete(const dpp::message_delete_t& event);
        void handle_on_message_delete_bulk(const dpp::message_delete_bulk_t& event);
    };
}

#endif
