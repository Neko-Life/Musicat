
#include <ogg/ogg.h>
#include <opus/opusfile.h>
#include <filesystem>
#include <sys/stat.h>
#include <regex>
#include "musicat/player.h"

namespace mc = musicat;
using string = std::string;

Sha_Track::Sha_Track() {};

Sha_Track::Sha_Track(YTrack t) {
    this->raw = t.raw;
}

Sha_Track::~Sha_Track() = default;

Sha_Player::Sha_Player(dpp::cluster * _cluster, dpp::snowflake _guild_id) {
    guild_id = _guild_id;
    cluster = _cluster;
    loop_mode = 0;
    shifted_track = 0;
    info_message = nullptr;
    queue = new std::deque<Sha_Track>();
}

Sha_Player::~Sha_Player() {
    delete queue;
    printf("Player destructor called\n");
}

Sha_Player& Sha_Player::add_track(Sha_Track track, bool top) {
    std::lock_guard<std::mutex> lk(this->q_m);
    if (top)
    {
        this->queue->push_front(track);
        std::lock_guard<std::mutex> lk(this->st_m);
        this->shifted_track++;
    }
    else this->queue->push_back(track);
    return *this;
}

bool Sha_Player::skip(dpp::voiceconn * v, dpp::snowflake user_id, int amount) {
    if (v && v->voiceclient && v->voiceclient->get_secs_remaining() > 0.1)
    {
        v->voiceclient->pause_audio(false);
        v->voiceclient->skip_to_next_marker();
        return true;
    }
    else return false;
}

bool Sha_Player::reset_shifted() {
    {
        std::lock_guard<std::mutex> lk(this->q_m);
        std::lock_guard<std::mutex> lk2(this->st_m);
        if (this->queue->size() && this->shifted_track > 0)
        {
            auto i = this->queue->begin() + this->shifted_track;
            auto s = *i;
            this->queue->erase(i);
            this->queue->push_front(s);
            this->shifted_track = 0;
            return true;
        }
        else return false;
    }
}

Sha_Player& Sha_Player::set_loop_mode(short int mode) {
    this->loop_mode = mode;
    return *this;
}

Sha_Player& Sha_Player::set_channel(dpp::snowflake channel_id) {
    std::lock_guard<std::mutex> lk(this->ch_m);
    this->channel_id = channel_id;
    return *this;
}

int Sha_Player::remove_track(int pos, int amount) {
    return 0;
}

int Sha_Player::remove_track_by_user(dpp::snowflake user_id, int amount) {
    return 0;
}

bool Sha_Player::pause(dpp::discord_client * from, dpp::snowflake user_id) {
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

int Sha_Player::seek(int pos, bool abs) {
    return 0;
}

int Sha_Player::stop() {
    return 0;
}

int Sha_Player::resume() {
    return 0;
}

int Sha_Player::search(string query) {
    return 0;
}

int Sha_Player::join() {
    return 0;
}

int Sha_Player::leave() {
    return 0;
}

int Sha_Player::rejoin() {
    return 0;
}

Sha_Player_Manager::Sha_Player_Manager(dpp::cluster * _cluster, dpp::snowflake _sha_id) {
    cluster = _cluster;
    sha_id = _sha_id;
    players = new std::map<dpp::snowflake, Sha_Player*>();
}

Sha_Player_Manager::~Sha_Player_Manager() {
    std::lock_guard<std::mutex> lk(ps_m);
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

Sha_Player* Sha_Player_Manager::create_player(dpp::snowflake guild_id) {
    std::lock_guard<std::mutex> lk(this->ps_m);
    auto l = players->find(guild_id);
    if (l != players->end()) return l->second;
    Sha_Player* v = new Sha_Player(cluster, guild_id);
    players->insert(std::pair(guild_id, v));
    return v;
}

Sha_Player* Sha_Player_Manager::get_player(dpp::snowflake guild_id) {
    std::lock_guard<std::mutex> lk(this->ps_m);
    auto l = players->find(guild_id);
    if (l != players->end()) return l->second;
    return NULL;
}

void Sha_Player_Manager::reconnect(dpp::discord_client * from, dpp::snowflake guild_id) {
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

bool Sha_Player_Manager::delete_player(dpp::snowflake guild_id) {
    std::lock_guard<std::mutex> lk(this->ps_m);
    auto l = players->find(guild_id);
    if (l == players->end()) return false;
    delete l->second;
    players->erase(l);
    return true;
}

std::deque<Sha_Track>* Sha_Player_Manager::get_queue(dpp::snowflake guild_id) {
    auto p = get_player(guild_id);
    if (!p) return nullptr;
    return p->queue;
}

bool Sha_Player_Manager::pause(dpp::discord_client * from, dpp::snowflake guild_id, dpp::snowflake user_id) {
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

bool Sha_Player_Manager::unpause(dpp::discord_voice_client * voiceclient, dpp::snowflake guild_id) {
    std::lock_guard<std::mutex> lk(this->mp_m);
    auto l = mc::vector_find(&this->manually_paused, guild_id);
    bool ret;
    if (l != this->manually_paused.end())
    {
        this->manually_paused.erase(l);
        ret = true;
    }
    else ret = false;
    voiceclient->pause_audio(false);
    return ret;
}

void Sha_Player_Manager::stop_stream(dpp::snowflake guild_id) {
    {
        std::lock_guard<std::mutex> lk(this->sq_m);
        if (mc::vector_find(&this->stop_queue, guild_id) == this->stop_queue.end())
            this->stop_queue.push_back(guild_id);
    }
}

bool Sha_Player_Manager::skip(dpp::voiceconn * v, dpp::snowflake guild_id, dpp::snowflake user_id) {
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
    if (p->loop_mode == 1 || p->loop_mode == 3)
    {
        std::lock_guard<std::mutex> lk(p->q_m);
        auto l = p->queue->front();
        p->queue->pop_front();
        if (p->loop_mode == 3) p->queue->push_back(l);
    }
    if (v && v->voiceclient && v->voiceclient->get_secs_remaining() > 0.1)
        this->stop_stream(guild_id);
    bool a = p->skip(v, user_id);
    return a;
}

void Sha_Player_Manager::download(string fname, string url, dpp::snowflake guild_id) {
    std::thread tj([this](string fname, string url, dpp::snowflake guild_id) {
        {
            std::lock_guard<std::mutex> lk(this->dl_m);
            this->waiting_file_download[fname] = guild_id;
        }
        {
            struct stat buf;
            if (stat("music", &buf) != 0)
                std::filesystem::create_directory("music");
        }
        string cmd = string("yt-dlp -f 251 -o - '") + url
            + string("' | ffmpeg -i - -b:a 384000 -ar 48000 -ac 2 -sn -vn -c libopus -f ogg 'music/")
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

void Sha_Player_Manager::wait_for_download(string file_name) {
    std::unique_lock<std::mutex> lk(this->dl_m);
    this->dl_cv.wait(lk, [this, file_name]() {
        return this->waiting_file_download.find(file_name) == this->waiting_file_download.end();
    });
}

void Sha_Player_Manager::stream(dpp::discord_voice_client * v, string fname) {
    dpp::snowflake server_id;
    if (v && !v->terminating && v->is_ready())
    {
        server_id = v->server_id;
        {
            struct stat buf;
            if (stat("music", &buf) != 0)
                std::filesystem::create_directory("music");
        }
        auto start_time = std::chrono::high_resolution_clock::now();
        printf("Streaming \"%s\" to %ld\n", fname.c_str(), server_id);
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
            {
                std::lock_guard<std::mutex> lk(this->sq_m);
                auto sq = mc::vector_find(&this->stop_queue, server_id);
                if (sq != this->stop_queue.end()) break;
            }
            if (!v || v->terminating)
            {
                fprintf(stderr, "[ERROR(sha_player.431)] Can't continue streaming, connection broken\n");
                break;
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
                    auto sq = mc::vector_find(&this->stop_queue, server_id);
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

                if (!v || v->terminating)
                {
                    fprintf(stderr, "[ERROR(sha_player.382)] Can't continue streaming, connection broken\n");
                    break;
                }

                /* Send the audio */
                int samples = opus_packet_get_samples_per_frame(op.packet, 48000);

                if (v && !v->terminating) v->send_audio_opus(op.packet, op.bytes, samples / 48);

                bool br = v->terminating;

                while (v && !v->terminating && v->get_secs_remaining() > 3.0)
                {
                    sleep(1);
                    if (v->terminating) br = true;
                }

                {
                    std::lock_guard<std::mutex> lk(this->sq_m);
                    auto sq = mc::vector_find(&this->stop_queue, server_id);
                    if (sq != this->stop_queue.end())
                    {
                        br = true;
                        break;
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
        if (server_id)
        {
            std::lock_guard<std::mutex> lk(this->sq_m);
            auto sq = mc::vector_find(&this->stop_queue, server_id);
            if (sq != this->stop_queue.end())
            {
                printf("ERASING QUEUE\n");
                this->stop_queue.erase(sq);
                this->stop_queue_cv.notify_all();
            }
        }
        if (v && !v->terminating) v->insert_marker("e");
    }
    else throw 1;
}

void Sha_Player_Manager::play(dpp::discord_voice_client * v, string fname) {
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

void Sha_Player_Manager::send_info_embed(dpp::snowflake guild_id) {
    {
        auto player = this->get_player(guild_id);
        if (!player) throw mc::exception("No player");
        auto channel_id = player->channel_id;
        dpp::embed e;
        try
        {
            e = get_embed(guild_id);
        }
        catch (mc::exception e)
        {
            fprintf(stderr, "[ERROR(player.646)] Failed to get_embed: %s\n", e.what().c_str());
            return;
        }
        catch (const dpp::exception& e)
        {
            fprintf(stderr, "[ERROR(player.646)] Failed to get_embed [dpp::exception]: %s\n", e.what());
            return;
        }
        catch (const std::logic_error& e)
        {
            fprintf(stderr, "[ERROR(player.646)] Failed to get_embed [std::logic_error]: %s\n", e.what());
            return;
        }
        dpp::message m;
        m.set_channel_id(channel_id)
            .add_embed(e);
        this->cluster->message_create(m, [player, guild_id, channel_id](dpp::confirmation_callback_t cb) {
            if (cb.is_error())
            {
                fprintf(stderr, "[ERROR(player.668)] message_create callback error:\nmes: %s\ncode: %d\nerr: err_vector\n", cb.get_error().message.c_str(), cb.get_error().code);
                return;
            }
            if (cb.value.index())
            {
                if (!player)
                {
                    printf("PLAYER GONE WTFF\n");
                    return;
                }
                player->info_message = &std::get<dpp::message>(cb.value);
                printf("New message info: %ld\n", player->info_message->id);
            }
            else printf("No message_create cb size\n");
        });
    }
}

bool Sha_Player_Manager::handle_on_track_marker(const dpp::voice_track_marker_t & event) {
    printf("Handling voice marker\n");
    if (!event.voice_client) { printf("NO CLIENT\n");return false; }
    {
        std::lock_guard lk(this->dc_m);
        if (this->disconnecting.find(event.voice_client->server_id) != this->disconnecting.end())
        {
            printf("DISCONNECTING\n");
            return false;
        }
    }
    auto p = this->get_player(event.voice_client->server_id);
    if (!p) { printf("NO PLAYER\n"); return false; }
    Sha_Track s;
    {
        if (p->queue->size() == 0) { printf("NO SIZE BEFORE: %d\n", p->loop_mode);return false; }

        // Handle shifted tracks (tracks shifted to the front of the queue)
        printf("Resetting shifted: %d\n", p->reset_shifted());

        std::lock_guard<std::mutex> lk(p->q_m);

        // Do stuff according to loop mode when playback ends
        if (event.track_meta == "e")
        {
            if (p->loop_mode == 0) p->queue->pop_front();
            else if (p->loop_mode == 2)
            {
                auto l = p->queue->front();
                p->queue->pop_front();
                p->queue->push_back(l);
            }
        }
        if (p->queue->size() == 0) { printf("NO SIZE AFTER: %d\n", p->loop_mode);return false; }
        s = p->queue->front();
    }

    printf("To play\n");
    if (event.voice_client && event.voice_client->get_secs_remaining() < 0.1)
    {
        printf("Starting thread\n");
        std::thread tj([this](dpp::discord_voice_client* v, Sha_Track track, string meta, Sha_Player* player) {
            bool timed_out = false;
            auto guild_id = v->server_id;
            dpp::snowflake channel_id = player->channel_id;
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
                auto a = this->waiting_file_download.find(track.filename);
                if (a != this->waiting_file_download.end())
                {
                    printf("Waiting for download\n");
                    this->dl_cv.wait(lk, [this, track, &timed_out]() {
                        auto t = this->waiting_file_download.find(track.filename);
                        // if (timed_out)
                        // {
                        //     this->waiting_file_download.erase(t);
                        //     return true;
                        // }
                        auto c = t == this->waiting_file_download.end();
                        printf("Checking for download: %s \"%s\"\n", c ? "DONE" : "DOWNLOADING", track.filename.c_str());
                        return c;
                    });
                }
            }
            if (timed_out) throw mc::exception("Operation took too long, aborted...", 0);
            if (meta == "r") v->send_silence(60);
            this->play(v, track.filename);

            // Send play info embed
            try
            {
                if (channel_id) this->send_info_embed(guild_id);
                else printf("No channel Id to send info embed\n");
            }
            catch (mc::exception e)
            {
                fprintf(stderr, "[ERROR(player.646)] %s\n", e.what().c_str());
            }
        }, event.voice_client, s, event.track_meta, p);
        tj.detach();
        return true;
    }
    else printf("TRACK SIZE\n");
    return false;
}

dpp::embed Sha_Player_Manager::get_embed(dpp::snowflake guild_id) {
    auto player = this->get_player(guild_id);
    if (!player) throw mc::exception("No player");

    // Reset shifted tracks
    player->reset_shifted();

    std::lock_guard<std::mutex> lk(player->q_m);
    if (!player->queue->size()) throw mc::exception("No player");
    auto track = player->queue->front();
    dpp::guild_member o = dpp::find_guild_member(guild_id, this->cluster->me.id);
    dpp::guild_member u = dpp::find_guild_member(guild_id, track.user_id);
    dpp::user* uc = dpp::find_user(u.user_id);
    uint32_t color = 0;
    for (auto i : o.roles)
    {
        auto r = dpp::find_role(i);
        if (!r || !r->colour) continue;
        color = r->colour;
        break;
    }
    string eaname = u.nickname.length() ? u.nickname : uc->username;
    dpp::embed_author ea;
    ea.name = eaname;
    auto ua = u.get_avatar_url(4096);
    auto uca = uc->get_avatar_url(4096);
    if (ua.length()) ea.icon_url = ua;
    else if (uca.length()) ea.icon_url = uca;

    static const char* l_mode[] = { "No repeat", "Repeat one", "Repeat queue", "Repeat one/queue" };
    string et = track.bestThumbnail().url;
    dpp::embed e;
    e.set_description(track.snippetText())
        .set_title(track.title())
        .set_url(track.url())
        .set_author(ea)
        .set_footer(l_mode[player->loop_mode], "");
    if (color)
        e.set_color(color);
    if (et.length())
        e.set_image(et);
    return e;
}

void Sha_Player_Manager::handle_on_voice_ready(const dpp::voice_ready_t & event) {
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
    auto l = event.voice_client->get_secs_remaining();
    printf("TO INSERT %d::%f\n", i, l);
    if (l < 0.1)
    {
        event.voice_client->insert_marker("r");
        printf("INSERTED\n");
    }
}

void Sha_Player_Manager::handle_on_voice_state_update(const dpp::voice_state_update_t & event) {
    // Non client's user code
    if (event.state.user_id != sha_id)
    {
        dpp::voiceconn* v = event.from->get_voice(event.state.guild_id);
        // Pause audio when no user listening in vc
        if (v && v->channel_id
            && v->channel_id != event.state.channel_id
            && v->voiceclient
            && v->voiceclient->get_secs_remaining() > 0.1
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
                && v->voiceclient->get_secs_remaining() > 0.1
                && v->voiceclient->is_paused())
            {
                std::lock_guard<std::mutex> lk(this->mp_m);
                // Whether the track paused automatically
                if (mc::vector_find(&this->manually_paused, event.state.guild_id) == this->manually_paused.end())
                {
                    v->voiceclient->send_silence(60);
                    v->voiceclient->pause_audio(false);
                }
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
