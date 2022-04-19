#include <deque>
#include <vector>
#include "yt-search.h"
#include "musicat.h"

#ifndef SHA_PLAYER_H
#define SHA_PLAYER_H

// TODO: Player class

class Sha_Player {
public:
    dpp::snowflake guild_id;
    int loop_mode;

    dpp::cluster* cluster;
    dpp::discord_client* client;
    std::deque<YTrack>* queue;
    dpp::voiceconn* voiceconn;

    // Must use this whenever doing the appropriate action
    std::mutex skip_mutex, seek_mutex, queue_mutex, play_mutex;

    Sha_Player(dpp::cluster* _cluster, dpp::snowflake _guild_id) {
        guild_id = _guild_id;
        cluster = _cluster;
        loop_mode = 0;
        queue = new std::deque<YTrack>();
    }

    ~Sha_Player() {
        delete queue;
        printf("Player destructor called\n");
    }

    dpp::voiceconn* set_voiceconn(dpp::voiceconn* _voiceconn) {
        voiceconn = _voiceconn;
        return voiceconn;
    }

    dpp::discord_client* set_client(dpp::discord_client* _client) {
        client = _client;
        return client;
    }

    int play(std::string _url) {
        return 0;
    }

    int add_track(bool _top) {
        return 0;
    }

    int skip(int _amount) {
        return 0;
    }

    int set_loop_mode(int _mode) {
        return 0;
    }

    int remove_track(int _pos, int _amount) {
        return 0;
    }

    int pause() {
        return 0;
    }

    int seek(int _pos, bool _abs) {
        return 0;
    }

    int stop() {
        return 0;
    }

    int resume() {
        return 0;
    }

    int search(std::string _query) {
        return 0;
    }

    int join() {
        return 0;
    }

    int leave() {
        return 0;
    }

    int rejoin() {
        return 0;
    }
};

class Sha_Player_Manager {
public:
    dpp::cluster* cluster;
    std::map<dpp::snowflake, Sha_Player*>* players;

    Sha_Player_Manager(dpp::cluster* _cluster) {
        cluster = _cluster;
        players = new std::map<dpp::snowflake, Sha_Player*>();
    }

    ~Sha_Player_Manager() {
        for (auto l = players->begin(); l != players->end(); l++)
        {
            if (l->second)
            {
                delete l->second;
                l->second = NULL;
            }
            players->erase(l);
        }
        delete players;
        players = NULL;
        printf("Player Manager destructor called\n");
    }

    /**
     * @brief Create a player object if not exist and return player
     *
     * @param _guild_id
     * @return Sha_Player*
     */
    Sha_Player* create_player(dpp::snowflake _guild_id) {
        auto l = players->find(_guild_id);
        if (l != players->end()) return l->second;
        Sha_Player* v = new Sha_Player(cluster, _guild_id);
        players->insert({ _guild_id, v });
        return v;
    }

    /**
     * @brief Get the player object, return NULL if not exist
     *
     * @param _guild_id
     * @return Sha_Player*
     */
    Sha_Player* get_player(dpp::snowflake _guild_id) {
        auto l = players->find(_guild_id);
        if (l != players->end()) return l->second;
        return NULL;
    }

    /**
     * @brief Return false if guild doesn't have player in the first place
     *
     * @param _guild_id
     * @return true
     * @return false
     */
    bool delete_player(dpp::snowflake _guild_id) {
        auto l = players->find(_guild_id);
        if (l == players->end()) return false;
        delete l->second;
        players->erase(l);
        return true;
    }
};

#endif
