#include "musicat/musicat.h"
#include "musicat/player.h"
#include <dirent.h>
#include <regex>
#include <sys/stat.h>
// #include <opus/opusfile.h>

namespace musicat
{
namespace player
{
// this section looks so bad
using string = std::string;

Manager::Manager (dpp::cluster *cluster, dpp::snowflake sha_id)
{
    this->cluster = cluster;
    this->sha_id = sha_id;
}

Manager::~Manager () = default;

std::shared_ptr<Player>
Manager::create_player (dpp::snowflake guild_id)
{
    std::lock_guard<std::mutex> lk (this->ps_m);
    auto l = players.find (guild_id);
    if (l != players.end ())
        return l->second;
    std::shared_ptr<Player> v = std::make_shared<Player> (cluster, guild_id);
    v->manager = this;
    players.insert (std::pair (guild_id, v));
    return v;
}

std::shared_ptr<Player>
Manager::get_player (dpp::snowflake guild_id)
{
    std::lock_guard<std::mutex> lk (this->ps_m);
    auto l = players.find (guild_id);
    if (l != players.end ())
        return l->second;
    return NULL;
}

void
Manager::reconnect (dpp::discord_client *from, dpp::snowflake guild_id)
{
    bool from_dc = false;
    {
        std::unique_lock<std::mutex> lk (this->dc_m);
        auto a = this->disconnecting.find (guild_id);
        if (a != this->disconnecting.end ())
            {
                from_dc = true;
                this->dl_cv.wait (lk, [this, &guild_id] () {
                    auto t = this->disconnecting.find (guild_id);
                    return t == this->disconnecting.end ();
                });
            }
    }
    {
        std::unique_lock<std::mutex> lk (this->c_m);
        auto a = this->connecting.find (guild_id);
        if (a != this->connecting.end ())
            {
                {
                    using namespace std::chrono_literals;
                    if (from_dc)
                        std::this_thread::sleep_for (500ms);
                }
                from->connect_voice (guild_id, a->second, false, true);
                this->dl_cv.wait (lk, [this, &guild_id] () {
                    auto t = this->connecting.find (guild_id);
                    return t == this->connecting.end ();
                });
            }
    }
}

bool
Manager::delete_player (dpp::snowflake guild_id)
{
    std::lock_guard<std::mutex> lk (this->ps_m);
    auto l = players.find (guild_id);
    if (l == players.end ())
        return false;
    players.erase (l);
    return true;
}

std::deque<MCTrack>
Manager::get_queue (dpp::snowflake guild_id)
{
    auto guild_player = get_player (guild_id);
    if (!guild_player)
        return {};
    guild_player->reset_shifted ();
    return guild_player->queue;
}

bool
Manager::pause (dpp::discord_client *from, dpp::snowflake guild_id,
                dpp::snowflake user_id)
{
    auto guild_player = get_player (guild_id);
    if (!guild_player)
        return false;
    bool a = guild_player->pause (from, user_id);
    if (a)
        {
            std::lock_guard<std::mutex> lk (mp_m);
            if (vector_find (&this->manually_paused, guild_id)
                == this->manually_paused.end ())
                this->manually_paused.push_back (guild_id);
            this->update_info_embed (guild_id);
        }
    return a;
}

bool
Manager::unpause (dpp::discord_voice_client *voiceclient,
                  dpp::snowflake guild_id)
{
    std::lock_guard<std::mutex> lk (this->mp_m);
    auto l = vector_find (&this->manually_paused, guild_id);
    bool ret;
    if (l != this->manually_paused.end ())
        {
            this->manually_paused.erase (l);
            ret = true;
        }
    else
        ret = false;
    voiceclient->pause_audio (false);
    this->update_info_embed (guild_id);
    return ret;
}

int
Manager::skip (dpp::voiceconn *v, dpp::snowflake guild_id,
               dpp::snowflake user_id, int64_t amount, bool remove)
{
    if (!v)
        return -1;
    auto guild_player = get_player (guild_id);
    if (!guild_player)
        return -1;

    const bool debug = get_debug_state();
    if (debug) printf("[Manager::skip] Locked player::t_mutex: %ld\n", guild_player->guild_id);
    std::lock_guard<std::mutex> lk (guild_player->t_mutex);
    try
        {
            auto u = get_voice_from_gid (guild_id, user_id);
            if (u.first->id != v->channel_id)
                throw exception ("You're not in my voice channel", 0);

            unsigned siz = 0;
            for (auto &i : u.second)
                {
                    auto &a = i.second;
                    if (a.is_deaf () || a.is_self_deaf ())
                        continue;
                    auto user = dpp::find_user (a.user_id);
                    if (user->is_bot ())
                        continue;
                    siz++;
                }
            // if (siz > 1U)
            // {
            //     std::lock_guard<std::mutex> lk(guild_player->q_m);
            //     auto& track = guild_player->queue.at(0);
            //     if (track.user_id != user_id && track.user_id !=
            //     this->sha_id)
            //     {
            //         amount = 1;
            //         bool exist = false;
            //         for (const auto& i : track.skip_vote)
            //         {
            //             if (i == user_id)
            //             {
            //                 exist = true;
            //                 break;
            //             }
            //         }
            //         if (!exist)
            //         {
            //             track.skip_vote.push_back(user_id);
            //         }

            //         unsigned ts = siz / 2U + 1U;
            //         size_t ret = track.skip_vote.size();
            //         if (ret < ts) return (int)ret;
            //         else track.skip_vote.clear();
            //     }
            //     else if (amount > 1)
            //     {
            //         int64_t count = 0;
            //         for (const auto& t : guild_player->queue)
            //         {
            //             if (t.user_id == user_id || t.user_id ==
            //             this->sha_id) count++; else break;

            //             if (amount == count) break;
            //         }
            //         if (amount > count) amount = count;
            //     }
            // }
        }
    catch (const char *e)
        {
            throw exception ("You're not in a voice channel", 1);
        }
    auto siz = guild_player->queue.size ();
    if (amount < (siz || 1))
        amount = siz || 1;
    if (amount > 1000)
        amount = 1000;
    const bool l_s = guild_player->loop_mode == loop_mode_t::l_song_queue;
    const bool l_q = guild_player->loop_mode == loop_mode_t::l_queue;
    {
        for (int64_t i = (guild_player->loop_mode == loop_mode_t::l_song || l_s) ? 0 : 1;
             i < amount; i++)
            {
                if (guild_player->queue.begin () == guild_player->queue.end ())
                    break;
                auto l = guild_player->queue.front ();
                guild_player->queue.pop_front ();
                if (!remove && (l_s || l_q))
                    guild_player->queue.push_back (l);
            }
    }
    if (v && v->voiceclient && v->voiceclient->get_secs_remaining () > 0.1)
        this->stop_stream (guild_id);

    int a = guild_player->skip (v);

    if (v && v->voiceclient && remove)
        v->voiceclient->insert_marker ("rm");

    if (debug) printf("[Manager::skip] Should unlock player::t_mutex: %ld\n", guild_player->guild_id);
    return a;
}

void
Manager::download (string fname, string url, dpp::snowflake guild_id)
{
    std::thread tj (
        [this] (string fname, string url, dpp::snowflake guild_id) {
            {
                std::lock_guard<std::mutex> lk (this->dl_m);
                this->waiting_file_download[fname] = guild_id;
            }

            const std::string music_folder_path = get_music_folder_path ();

            {
                struct stat buf;
                if (stat (music_folder_path.c_str (), &buf) != 0)
                    std::filesystem::create_directory (music_folder_path);
            }

            string cmd
                = string ("yt-dlp -f 251 --http-chunk-size 2M '") + url
                  + string ("' -x --audio-format opus --audio-quality 0 -o '")
                  + music_folder_path
                  + std::regex_replace (fname, std::regex ("(')"), "'\\''",
                                        std::regex_constants::match_any)
                  + string ("'");

            const bool debug = get_debug_state ();

            if (debug)
                {
                    printf ("DOWNLOAD: \"%s\" \"%s\"\n", fname.c_str (),
                            url.c_str ());
                    printf ("CMD: %s\n", cmd.c_str ());
                }
            else
                cmd += " 1>/dev/null";

            system (cmd.c_str ());

            {
                std::lock_guard<std::mutex> lk (this->dl_m);
                this->waiting_file_download.erase (fname);
            }

            this->dl_cv.notify_all ();
        },
        fname, url, guild_id);
    tj.detach ();
}

void
Manager::play (dpp::discord_voice_client *v, player::MCTrack &track,
               dpp::snowflake channel_id, bool notify_error)
{
    std::thread tj (
        [this, &track] (dpp::discord_voice_client *v,
                dpp::snowflake channel_id, bool notify_error) {
            const bool debug = get_debug_state ();

            if (debug)
                printf ("Attempt to stream\n");
            auto server_id = v->server_id;
            auto voice_channel_id = v->channel_id;

            try
                {
                    this->stream (v, track);
                }
            catch (int e)
                {
                    fprintf (stderr,
                             "[ERROR MANAGER::PLAY] Stream thrown error with "
                             "code: %d\n",
                             e);

                    if (notify_error)
                        {
                            string msg = "";

                            // Maybe connect/reconnect here if there's
                            // connection error
                            if (e == 2)
                                msg = "Can't start playback";
                            else if (e == 1)
                                msg = "No connection";

                            dpp::message m;
                            m.set_channel_id (channel_id).set_content (msg);

                            this->cluster->message_create (m);
                        }
                }

            if (server_id)
                {
                    std::lock_guard<std::mutex> lk (this->sq_m);

                    auto sq = vector_find (&this->stop_queue, server_id);
                    while (sq != this->stop_queue.end ())
                        {
                            if (debug)
                                printf ("[MANAGER::STREAM] Stopped because "
                                        "stop query, cleaning up query: %ld\n", server_id);
                            this->stop_queue.erase (sq);
                            this->stop_queue_cv.notify_all ();
                            sq = vector_find (&this->stop_queue, server_id);
                        }
                }

            if (v && !v->terminating)
                v->insert_marker ("e");
            else
                {
                    try
                        {
                            get_voice_from_gid (server_id, this->sha_id);
                            return;
                        }
                    catch (...)
                        {
                        }

                    if (server_id && voice_channel_id)
                        {
                            std::lock_guard<std::mutex> lk (this->c_m);
                            this->connecting[server_id] = voice_channel_id;
                        }
                    // if (v) v->~discord_voice_client();
                }
        },
        v, channel_id, notify_error);
    tj.detach ();
}

size_t
Manager::remove_track (dpp::snowflake guild_id, size_t pos,
                       const size_t amount, const size_t to)
{
    auto guild_player = this->get_player (guild_id);
    if (!guild_player)
        return 0;
    return guild_player->remove_track (pos, amount, to);
}

bool
Manager::shuffle_queue (dpp::snowflake guild_id)
{
    auto guild_player = this->get_player (guild_id);
    if (!guild_player)
        return false;
    return guild_player->shuffle ();
}

} // player
} // musicat
