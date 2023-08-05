#include "musicat/musicat.h"
#include "musicat/player.h"
#include <chrono>
#include <dirent.h>
#include <regex>
#include <sys/stat.h>
#include <thread>
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

std::pair<std::deque<MCTrack>, int>
Manager::skip (dpp::voiceconn *v, dpp::snowflake guild_id,
               dpp::snowflake user_id, int64_t amount, bool remove)
{
    if (!v)
        return { {}, -1 };

    auto guild_player = get_player (guild_id);
    if (!guild_player)
        return { {}, -1 };

    const bool debug = get_debug_state ();
    if (debug)
        printf ("[Manager::skip] Locked player::t_mutex: %ld\n",
                guild_player->guild_id);

    std::lock_guard<std::mutex> lk (guild_player->t_mutex);
    try
        {
            auto u = get_voice_from_gid (guild_id, user_id);
            if (u.first->id != v->channel_id)
                throw exception ("You're not in my voice channel", 0);

            // some vote logic here but decided to disable it
            // cuz i remember someone told me it's incovenient
            // also some logic is not handled properly

            // unsigned siz = 0;
            // for (auto &i : u.second)
            //     {
            //         auto &a = i.second;
            //         if (a.is_deaf () || a.is_self_deaf ())
            //             continue;
            //         auto user = dpp::find_user (a.user_id);
            //         if (user->is_bot ())
            //             continue;
            //         siz++;
            //     }

            // if (siz > 1U)
            // {
            //     std::lock_guard<std::mutex> lk(guild_player->q_m);
            //     auto& track = guild_player->queue.at(0);
            //     auto& track = guild_player->current_track;
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

    auto removed_tracks = guild_player->skip_queue (amount, remove);

    if (v && v->voiceclient && v->voiceclient->get_secs_remaining () > 0.05f)
        this->stop_stream (guild_id);

    auto a = guild_player->skip (v);
    if (a.first.size ())
        {
            for (auto &e : a.first)
                removed_tracks.push_back (e);
        }

    if (remove && !guild_player->stopped && v && v->voiceclient)
        v->voiceclient->insert_marker ("rm");

    if (debug)
        printf ("[Manager::skip] Should unlock player::t_mutex: %ld\n",
                guild_player->guild_id);

    return { removed_tracks, a.second };
}

void
Manager::download (const string &fname, const string &url,
                   const dpp::snowflake &guild_id)
{
    const string yt_dlp = get_ytdlp_exe ();
    if (!yt_dlp.length ())
        {
            fprintf (stderr,
                     "[ERROR Manager::download] yt-dlp executable isn't "
                     "configured, unable to download track '%s'\n",
                     fname.c_str ());
            return;
        }

    std::thread tj (
        [this, yt_dlp] (string fname, string url, dpp::snowflake guild_id) {
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
                = string (yt_dlp + " -f 251 --http-chunk-size 2M '") + url
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

            track.stopping = false;

            if (v && !v->terminating)
                {
                    v->insert_marker ("e");
                }
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
