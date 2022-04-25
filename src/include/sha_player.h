#include <deque>
#include <vector>
#include "yt-search.h"
#include "musicat.h"

#ifndef SHA_PLAYER_H
#define SHA_PLAYER_H

using string = std::string;

// TODO: Player class

struct Sha_Track : YTrack {
    string filename;
    dpp::snowflake user_id;

    Sha_Track() {};

    Sha_Track(YTrack t) {
        this->raw = t.raw;
    }
};

class Sha_Player {
public:
    dpp::snowflake guild_id;
    int loop_mode;

    dpp::cluster* cluster;
    std::deque<Sha_Track>* queue;

    // Must use this whenever doing the appropriate action
    // q_m: queue
    std::mutex skip_mutex, seek_mutex, q_m, play_mutex;

    Sha_Player(dpp::cluster* _cluster, dpp::snowflake _guild_id) {
        guild_id = _guild_id;
        cluster = _cluster;
        loop_mode = 0;
        queue = new std::deque<Sha_Track>();
    }

    ~Sha_Player() {
        delete queue;
        printf("Player destructor called\n");
    }

    Sha_Player* add_track(Sha_Track track, bool top = false) {
        std::lock_guard<std::mutex> lk(this->q_m);
        queue->push_back(track);
        return this;
    }

    bool skip(dpp::voiceconn* v, dpp::snowflake user_id, int amount = 1) {
        if (v && v->voiceclient && v->voiceclient->get_tracks_remaining() > 0)
        {
            v->voiceclient->pause_audio(false);
            v->voiceclient->skip_to_next_marker();
            return true;
        }
        else return false;
    }

    int set_loop_mode(int _mode) {
        return 0;
    }

    int remove_track(int pos, int amount = 1) {
        return 0;
    }

    int remove_track_by_user(dpp::snowflake user_id, int amount = -1) {
        return 0;
    }

    bool pause(dpp::discord_client* from, dpp::snowflake user_id) {
        auto v = from->get_voice(guild_id);
        if (v && !v->voiceclient->is_paused())
        {
            try
            {
                auto u = mc::get_voice_from_gid(guild_id, user_id);
                if (u.first->id != v->channel_id) throw mc::exception("You're not in my voice channel", 0);
            }
            catch (const char* e)
            {
                throw mc::exception("You're not in a voice channel", 1);
            }
            v->voiceclient->pause_audio(true);
            // Paused
            return true;
        }
        // Not playing anythin
        else return false;
    }

    int seek(int pos, bool abs) {
        return 0;
    }

    int stop() {
        return 0;
    }

    int resume() {
        return 0;
    }

    int search(string query) {
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
    dpp::snowflake sha_id;
    // Mutexes
    // dl: waiting_file_download
    // wd: waiting_vc_ready
    // c: connecting
    // dc: disconnecting
    // wm: waiting_marker
    // mp: manually_paused
    // sq: stop_queue
    std::mutex dl_m, wd_m, c_m, dc_m, wm_m, mp_m, sq_m;
    // Conditional variable, use notify_all
    std::condition_variable dl_cv, stop_queue_cv;
    std::map<uint64_t, uint64_t> connecting, disconnecting;
    std::map<uint64_t, string> waiting_vc_ready;
    std::map<string, uint64_t> waiting_file_download;
    std::map<uint64_t, std::vector<string>> waiting_marker;
    std::vector<uint64_t> manually_paused;
    std::vector<dpp::snowflake> stop_queue;

    Sha_Player_Manager(dpp::cluster* _cluster, dpp::snowflake _sha_id) {
        cluster = _cluster;
        sha_id = _sha_id;
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
     * @param guild_id
     * @return Sha_Player*
     */
    Sha_Player* create_player(dpp::snowflake guild_id) {
        auto l = players->find(guild_id);
        if (l != players->end()) return l->second;
        Sha_Player* v = new Sha_Player(cluster, guild_id);
        players->insert(std::pair(guild_id, v));
        return v;
    }

    /**
     * @brief Get the player object, return NULL if not exist
     *
     * @param guild_id
     * @return Sha_Player*
     */
    Sha_Player* get_player(dpp::snowflake guild_id) {
        auto l = players->find(guild_id);
        if (l != players->end()) return l->second;
        return NULL;
    }

    void reconnect(dpp::discord_client* from, dpp::snowflake guild_id) {
        {
            std::unique_lock lk(this->dc_m);
            auto a = this->disconnecting.find(guild_id);
            if (a != this->disconnecting.end())
            {
                this->dl_cv.wait(lk, [this, &guild_id]() {
                    auto t = this->disconnecting.find(guild_id);
                    return t == this->disconnecting.end();
                });
            }
        }
        {
            std::unique_lock lk(this->c_m);
            auto a = this->connecting.find(guild_id);
            if (a != this->connecting.end())
            {
                from->connect_voice(guild_id, a->second);
                this->dl_cv.wait(lk, [this, &guild_id]() {
                    auto t = this->connecting.find(guild_id);
                    return t == this->connecting.end();
                });
            }
        }
    }

    /**
     * @brief Return false if guild doesn't have player in the first place
     *
     * @param guild_id
     * @return true
     * @return false
     */
    bool delete_player(dpp::snowflake guild_id) {
        auto l = players->find(guild_id);
        if (l == players->end()) return false;
        delete l->second;
        players->erase(l);
        return true;
    }

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
    bool pause(dpp::discord_client* from, dpp::snowflake guild_id, dpp::snowflake user_id) {
        auto p = get_player(guild_id);
        if (!p) return false;
        bool a = p->pause(from, user_id);
        if (a)
        {
            std::lock_guard<std::mutex> lk(mp_m);
            if (mc::vector_find(&this->manually_paused, guild_id) == this->manually_paused.end())
                this->manually_paused.push_back(guild_id);
        }
        return a;
    }

    void stop_stream(dpp::snowflake guild_id) {
        {
            std::lock_guard<std::mutex> lk(this->sq_m);
            if (mc::vector_find(&this->stop_queue, guild_id) == this->stop_queue.end())
                this->stop_queue.push_back(guild_id);
        }
    }

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
    bool skip(dpp::voiceconn* v, dpp::snowflake guild_id, dpp::snowflake user_id) {
        auto p = get_player(guild_id);
        if (!p) return false;
        try
        {
            auto u = mc::get_voice_from_gid(guild_id, user_id);
            if (u.first->id != v->channel_id) throw mc::exception("You're not in my voice channel", 0);
        }
        catch (const char* e)
        {
            throw mc::exception("You're not in a voice channel", 1);
        }
        if (v && v->voiceclient && v->voiceclient->get_tracks_remaining() > 0)
            this->stop_stream(guild_id);
        bool a = p->skip(v, user_id);
        return a;
    }

    void download(string fname, string url, dpp::snowflake guild_id) {
        std::thread tj([this](string fname, string url, dpp::snowflake guild_id) {
            {
                std::lock_guard<std::mutex> lk(this->dl_m);
                this->waiting_file_download[fname] = guild_id;
            }
            string cmd = string("yt-dlp -f 251 -o - '") + url
                + string("' | ffmpeg -i - -ar 48000 -ac 2 -sn -vn -c libopus -f ogg 'music/")
                + std::regex_replace(
                    fname, std::regex("(')"), "'\\''",
                    std::regex_constants::match_any)
                + string("'");
            printf("DOWNLOAD: \"%s\" \"%s\"\n", fname.c_str(), url.c_str());
            printf("CMD: %s\n", cmd.c_str());
            FILE* a = popen(cmd.c_str(), "w");
            pclose(a);
            {
                std::lock_guard<std::mutex> lk(this->dl_m);
                this->waiting_file_download.erase(fname);
            }
            this->dl_cv.notify_all();
        }, fname, url, guild_id);
        tj.detach();
    }


    void stream(dpp::discord_voice_client* v, string fname) {
        if (v && v->is_ready())
        {
            auto start_time = std::chrono::high_resolution_clock::now();
            printf("Streaming \"%s\" to %ld\n", fname.c_str(), v->server_id);
            {
                std::ifstream fs((string("music/") + fname).c_str());
                if (!fs.is_open()) throw 2;
                else fs.close();
            }
            FILE* fd = fopen((string("music/") + fname).c_str(), "rb");

            printf("Initializing buffer\n");
            ogg_sync_state oy;
            ogg_stream_state os;
            ogg_page og;
            ogg_packet op;
            // OpusHead header;
            char* buffer;

            fseek(fd, 0L, SEEK_END);
            size_t sz = ftell(fd);
            printf("SIZE_T: %ld\n", sz);
            rewind(fd);

            ogg_sync_init(&oy);

            // int eos = 0;
            // int i;

            buffer = ogg_sync_buffer(&oy, sz);
            fread(buffer, 1, sz, fd);

            ogg_sync_wrote(&oy, sz);

            if (ogg_sync_pageout(&oy, &og) != 1)
            {
                fprintf(stderr, "Does not appear to be ogg stream.\n");
                exit(1);
            }

            ogg_stream_init(&os, ogg_page_serialno(&og));

            if (ogg_stream_pagein(&os, &og) < 0)
            {
                fprintf(stderr, "Error reading initial page of ogg stream.\n");
                exit(1);
            }

            if (ogg_stream_packetout(&os, &op) != 1)
            {
                fprintf(stderr, "Error reading header packet of ogg stream.\n");
                exit(1);
            }

            /* We must ensure that the ogg stream actually contains opus data */
            // if (!(op.bytes > 8 && !memcmp("OpusHead", op.packet, 8)))
            // {
            //     fprintf(stderr, "Not an ogg opus stream.\n");
            //     exit(1);
            // }

            // /* Parse the header to get stream info */
            // int err = opus_head_parse(&header, op.packet, op.bytes);
            // if (err)
            // {
            //     fprintf(stderr, "Not a ogg opus stream\n");
            //     exit(1);
            // }
            // /* Now we ensure the encoding is correct for Discord */
            // if (header.channel_count != 2 && header.input_sample_rate != 48000)
            // {
            //     fprintf(stderr, "Wrong encoding for Discord, must be 48000Hz sample rate with 2 channels.\n");
            //     exit(1);
            // }

            /* Now loop though all the pages and send the packets to the vc */
            while (ogg_sync_pageout(&oy, &og) == 1)
            {
                if (!v)
                {
                    fprintf(stderr, "[ERROR(sha_player.319)] Can't continue streaming, connection broken\n");
                    break;
                }
                {
                    std::lock_guard<std::mutex> lk(this->sq_m);
                    auto sq = mc::vector_find(&this->stop_queue, v->server_id);
                    if (sq != this->stop_queue.end())
                    {
                        printf("ERASING QUEUE\n");
                        this->stop_queue.erase(sq);
                        this->stop_queue_cv.notify_all();
                        break;
                    }
                }
                while (ogg_stream_check(&os) != 0)
                {
                    ogg_stream_init(&os, ogg_page_serialno(&og));
                }

                if (ogg_stream_pagein(&os, &og) < 0)
                {
                    fprintf(stderr, "Error reading page of Ogg bitstream data.\n");
                    exit(1);
                }

                int res;

                while ((res = ogg_stream_packetout(&os, &op)) != 0)
                {
                    {
                        std::lock_guard<std::mutex> lk(this->sq_m);
                        auto sq = mc::vector_find(&this->stop_queue, v->server_id);
                        if (sq != this->stop_queue.end()) break;
                    }
                    if (res < 1)
                    {
                        ogg_stream_pagein(&os, &og);
                        continue;
                    }
                    // /* Read remaining headers */
                    // if (op.bytes > 8 && !memcmp("OpusHead", op.packet, 8))
                    // {
                    //     int err = opus_head_parse(&header, op.packet, op.bytes);
                    //     if (err)
                    //     {
                    //         fprintf(stderr, "Not a ogg opus stream\n");
                    //         exit(1);
                    //     }
                    //     if (header.channel_count != 2 && header.input_sample_rate != 48000)
                    //     {
                    //         fprintf(stderr, "Wrong encoding for Discord, must be 48000Hz sample rate with 2 channels.\n");
                    //         exit(1);
                    //     }
                    //     continue;
                    // }
                    /* Skip the opus tags */
                    if (op.bytes > 8 && !memcmp("OpusTags", op.packet, 8))
                        continue;

                    if (!v)
                    {
                        fprintf(stderr, "[ERROR(sha_player.382)] Can't continue streaming, connection broken\n");
                        break;
                    }

                    /* Send the audio */
                    int samples = opus_packet_get_samples_per_frame(op.packet, 48000);

                    v->send_audio_opus(op.packet, op.bytes, samples / 48);

                    bool br = false;

                    while (v && v->get_secs_remaining() > 3.0)
                    {
                        sleep(1);
                        {
                            std::lock_guard<std::mutex> lk(this->sq_m);
                            auto sq = mc::vector_find(&this->stop_queue, v->server_id);
                            if (sq != this->stop_queue.end())
                            {
                                br = true;
                                break;
                            }
                        }
                    }

                    if (br)
                    {
                        printf("Breaking\n");
                        break;
                    }
                }
            }

            /* Cleanup */
            fclose(fd);
            ogg_stream_clear(&os);
            ogg_sync_clear(&oy);
            auto end_time = std::chrono::high_resolution_clock::now();
            auto done = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            printf("Done streaming for %ld milliseconds\n", done.count());
            v->insert_marker();
        }
        else throw 1;
    }

    void play(dpp::discord_voice_client* v, string fname) {
        std::thread tj([this](dpp::discord_voice_client* v, string fname) {
            printf("Attempt to stream\n");
            // dpp::voiceconn* v = from->get_voice(guild_id);
            // {
            //     std::unique_lock<std::mutex> lk(wm_m);
            //     if (waiting_marker.find(event.msg.guild_id) == waiting_marker.end())
            //     {
            //         std::vector<string> a;
            //         waiting_marker.insert({ event.msg.guild_id, a });
            //     }
            //     waiting_marker[event.msg.guild_id].push_back(fname);
            //     this->dl_cv.wait(lk, [&waiting_marker, fname, event]() {
            //         return true;
            //         auto l = waiting_marker.find(event.msg.guild_id);
            //         for (int i = 0; i < (int)(l->second.size()); i++)
            //             if (!l->second.at(i).empty())
            //                 if (l->second.at(i) == fname) return true;
            //                 else return false;
            //         return false;
            //     });
            // }

            // if (!v)
            try
            {
                this->stream(v, fname);
            }
            catch (int e)
            {
                printf("ERROR_CODE: %d\n", e);
                if (e == 2) throw mc::exception("Can't access file", 2);
                else if (e == 1) throw mc::exception("No connection", 1);
            }

        }, v, fname);
        tj.detach();
    }

    void handle_on_track_marker(const dpp::voice_track_marker_t& event) {
        if (!event.voice_client) { printf("NO CLIENT\n");return; }
        auto p = this->get_player(event.voice_client->server_id);
        Sha_Track s;
        {
            std::lock_guard<std::mutex> lk(p->q_m);
            if (p->queue->size() == 0) { printf("NO SIZE\n");return; }
            s = p->queue->front();
            // TODO: Do stuff according to loop mode
            p->queue->pop_front();
        }
        // dpp::voiceconn* v = event.from->get_voice(event.voice_client->server_id);
        if (event.voice_client && event.voice_client->get_tracks_remaining() == 0)
        {
            std::thread tj([this](dpp::discord_voice_client* v, string fname) {
                bool timed_out = false;
                auto guild_id = v->server_id;
                // std::thread tmt([this](bool* _v) {
                //     int _w = 30;
                //     while (_v && *_v == false && _w > 0)
                //     {
                //         sleep(1);
                //         --_w;
                //     }
                //     if (_w) return;
                //     if (_v)
                //     {
                //         *_v = true;
                //         this->dl_cv.notify_all();
                //     }
                // }, &timed_out);
                // tmt.detach();
                {
                    std::unique_lock<std::mutex> lk(this->wd_m);
                    auto a = this->waiting_vc_ready.find(guild_id);
                    if (a != this->waiting_vc_ready.end())
                    {
                        printf("Waiting for ready state\n");
                        this->dl_cv.wait(lk, [this, &guild_id, &timed_out]() {
                            auto t = this->waiting_vc_ready.find(guild_id);
                            // if (timed_out)
                            // {
                            //     this->waiting_vc_ready.erase(t);
                            //     return true;
                            // }
                            auto c = t == this->waiting_vc_ready.end();
                            printf("Checking for ready state: %d\n", c);
                            return c;
                        });
                    }
                }
                {
                    std::unique_lock<std::mutex> lk(this->dl_m);
                    auto a = this->waiting_file_download.find(fname);
                    if (a != this->waiting_file_download.end())
                    {
                        printf("Waiting for download\n");
                        this->dl_cv.wait(lk, [this, fname, &timed_out]() {
                            auto t = this->waiting_file_download.find(fname);
                            // if (timed_out)
                            // {
                            //     this->waiting_file_download.erase(t);
                            //     return true;
                            // }
                            auto c = t == this->waiting_file_download.end();
                            printf("Checking for download: %s \"%s\"\n", c ? "DONE" : "DOWNLOADING", fname.c_str());
                            return c;
                        });
                    }
                }
                if (timed_out) throw mc::exception("Operation took too long, aborted...", 0);
                this->play(v, fname);
            }, event.voice_client, s.filename);
            tj.detach();
        }
        else printf("TRACK SIZE\n");
    }

    void handle_on_voice_ready(const dpp::voice_ready_t& event) {
        {
            std::lock_guard<std::mutex> lk(this->wd_m);
            printf("on_voice_ready\n");
            if (this->waiting_vc_ready.find(event.voice_client->server_id) != this->waiting_vc_ready.end())
            {
                this->waiting_vc_ready.erase(event.voice_client->server_id);
                this->dl_cv.notify_all();
            }
        }
        auto i = event.voice_client->get_tracks_remaining();
        printf("TO INSERT %d\n", i);
        if (i == 1)
        {
            event.voice_client->insert_marker();
            printf("INSERTED\n");
        }
    }

    void handle_on_voice_state_update(const dpp::voice_state_update_t& event) {
        // Non client's user code
        if (event.state.user_id != sha_id)
        {
            dpp::voiceconn* v = event.from->get_voice(event.state.guild_id);
            // Pause audio when no user listening in vc
            if (v && v->channel_id
                && v->channel_id != event.state.channel_id
                && v->voiceclient
                && v->voiceclient->get_tracks_remaining() > 0
                && !v->voiceclient->is_paused())
            {
                std::lock_guard<std::mutex> lk(this->mp_m);
                if (event.from
                    && mc::vector_find(&this->manually_paused, event.state.guild_id) == this->manually_paused.end())
                {
                    try
                    {
                        auto voice = mc::get_voice_from_gid(event.state.guild_id, sha_id);
                        // Whether there's human listening in the vc
                        bool p = false;
                        for (auto l : voice.second)
                        {
                            // This only check user in cache, if user not in cache then skip
                            auto a = dpp::find_user(l.first);
                            if (a)
                            {
                                if (!a->is_bot())
                                {
                                    p = true;
                                    break;
                                }
                            }
                        }
                        if (!p)
                        {
                            v->voiceclient->pause_audio(true);
                            printf("Paused %ld as no user in vc\n", event.state.guild_id);
                        }
                    }
                    catch (const char* e)
                    {
                        printf("ERROR(main.405) %s\n", e);
                    }
                }
            }
            else
            {
                if (v && v->channel_id
                    && v->channel_id == event.state.channel_id
                    && v->voiceclient
                    && v->voiceclient->get_tracks_remaining() > 0
                    && v->voiceclient->is_paused())
                {
                    std::lock_guard<std::mutex> lk(this->mp_m);
                    // Whether the track paused automatically
                    if (mc::vector_find(&this->manually_paused, event.state.guild_id) == this->manually_paused.end())
                        v->voiceclient->pause_audio(false);
                }

            }

            // End non client's user code
            return;
        }

        // Client user code -----------------------------------

        if (!event.state.channel_id)
        {
            {
                std::lock_guard lk(this->dc_m);
                printf("on_voice_state_leave\n");
                if (this->disconnecting.find(event.state.guild_id) != this->disconnecting.end())
                {
                    this->disconnecting.erase(event.state.guild_id);
                    this->dl_cv.notify_all();
                }
            }
        }
        else
        {
            {
                std::lock_guard lk(this->c_m);
                printf("on_voice_state_join\n");
                if (this->connecting.find(event.state.guild_id) != this->connecting.end())
                {
                    this->connecting.erase(event.state.guild_id);
                    this->dl_cv.notify_all();
                }
            }
        }
    }
};

#endif
