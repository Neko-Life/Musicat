#include "musicat/child/command.h"
#include "musicat/child/dl_music.h"
#include "musicat/musicat.h"
#include "musicat/player.h"
#include "musicat/thread_manager.h"
#include "musicat/util.h"
#include "musicat/util/base64.h"
#include <dirent.h>
#include <memory>
#include <thread>

#include <sys/stat.h>
#include <time.h>
#include <utime.h>

#define ENABLE_DAVE true
#define SELF_DEAF true

namespace musicat::player
{
// this section looks so bad
using string = std::string;

Manager::Manager (dpp::cluster *cluster) { this->cluster = cluster; }

Manager::~Manager () = default;

std::shared_ptr<Player>
Manager::create_player (const dpp::snowflake &guild_id)
{
    std::lock_guard lk (this->ps_m);

    auto l = players.find (guild_id);
    if (l != players.end ())
        return l->second;

    std::shared_ptr<Player> v = std::make_shared<Player> (cluster, guild_id);
    v->manager = this;
    players.insert (std::pair (guild_id, v));

    return v;
}

std::shared_ptr<Player>
Manager::get_player (const dpp::snowflake &guild_id)
{
    std::lock_guard lk (this->ps_m);

    auto l = players.find (guild_id);
    if (l != players.end ())
        return l->second;

    return NULL;
}

void
Manager::reconnect (dpp::discord_client *from, const dpp::snowflake &guild_id)
{
    bool from_dc = false;
    {
        std::unique_lock lk (this->dc_m);
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
        std::unique_lock lk (this->c_m);
        auto a = this->connecting.find (guild_id);
        if (a != this->connecting.end ())
            {
                {
                    using namespace std::chrono_literals;

                    // wait for 500 ms since discord will just ignore the
                    // request if it was too quick
                    if (from_dc)
                        std::this_thread::sleep_for (500ms);
                }

                from->connect_voice (guild_id, a->second, false, SELF_DEAF,
                                     ENABLE_DAVE);

                this->dl_cv.wait (lk, [this, &guild_id] () {
                    auto t = this->connecting.find (guild_id);
                    return t == this->connecting.end ();
                });
            }
    }
}

bool
Manager::delete_player (const dpp::snowflake &guild_id)
{
    std::lock_guard lk (this->ps_m);

    auto l = players.find (guild_id);
    if (l == players.end ())
        return false;

    players.erase (l);
    return true;
}

std::deque<MCTrack>
Manager::get_queue (const dpp::snowflake &guild_id)
{
    auto guild_player = get_player (guild_id);
    if (!guild_player)
        return {};

    guild_player->reset_shifted ();
    return guild_player->queue;
}

bool
Manager::pause (dpp::discord_client *from, const dpp::snowflake &guild_id,
                const dpp::snowflake &user_id, bool update_info_embed)
{
    auto guild_player = get_player (guild_id);
    if (!guild_player)
        return false;

    bool a = guild_player->pause (from, user_id);

    if (!a)
        return a;

    this->set_manually_paused (guild_id);

    if (update_info_embed)
        this->update_info_embed (guild_id);

    return a;
}

void
Manager::unpause (dpp::discord_voice_client *voiceclient,
                  const dpp::snowflake &guild_id, bool update_info_embed)
{
    this->clear_manually_paused (guild_id);

    if (voiceclient)
        {
            voiceclient->pause_audio (false);
        }

    if (update_info_embed)
        this->update_info_embed (guild_id);
}

std::pair<std::deque<MCTrack>, int>
Manager::skip (dpp::voiceconn *v, const dpp::snowflake &guild_id,
               const dpp::snowflake &user_id, const int64_t &amount,
               const bool remove)
{
    if (!v)
        return { {}, -1 };

    auto guild_player = get_player (guild_id);
    if (!guild_player)
        return { {}, -1 };

    guild_player->reset_shifted ();

    std::lock_guard lk (guild_player->t_mutex);

    auto u = get_voice_from_gid (guild_id, user_id);
    if (!u.first)
        throw exception ("You're not in a voice channel", 1);

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
    //     std::lock_guard lk(guild_player->q_m);
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

    auto removed_tracks = guild_player->skip_queue (amount, remove);

    bool stopping = false;
    if (v && v->voiceclient && v->voiceclient->get_secs_remaining () > 0.05f)
        stopping = this->stop_stream (guild_id) == 0;

    auto [removed_ts, status] = guild_player->skip (v);
    if (removed_ts.size ())
        {
            for (const auto &e : removed_ts)
                removed_tracks.push_back (e);
        }

    if (remove && !guild_player->stopped && v && v->voiceclient)
        v->voiceclient->insert_marker ("rm");

    if (status == 0 && !stopping)
        v->voiceclient->insert_marker ("e");

    return { removed_tracks, status };
}

int
read_notif_fifo (int notif_fifo, const std::string &filepath,
                 const std::string &url)
{
    char buf[4097];
    ssize_t cur_read = 0;
    bool has_progress = false;

    while ((cur_read = read (notif_fifo, &buf, 4096)) > 0)
        {
            buf[cur_read] = '\0';
            // log this for nice statistic or smt later
            fprintf (stderr, "%s%s", buf,
                     buf[cur_read - 1] == '\n' ? "" : "\n");

            has_progress = true;
        }

    return has_progress ? 0 : 1;
}

void
Manager::download (const string &fname, const string &url,
                   const dpp::snowflake &guild_id)
{
    const string yt_dlp = get_ytdlp_exe ();
    if (yt_dlp.empty ())
        {
            fprintf (stderr,
                     "[ERROR Manager::download] yt-dlp executable isn't "
                     "configured, unable to download track '%s'\n",
                     fname.c_str ());

            return;
        }

    std::thread tj (
        [this, yt_dlp] (string fname, string url, dpp::snowflake guild_id) {
            thread_manager::DoneSetter tmds;
            {
                std::lock_guard lk (this->dl_m);
                this->waiting_file_download[fname] = guild_id;
            }

            const string music_folder_path = get_music_folder_path ();

            {
                struct stat buf;
                if (stat (music_folder_path.c_str (), &buf) != 0)
                    std::filesystem::create_directory (music_folder_path);
            }

            const string filepath = music_folder_path + fname;
            const bool debug = get_debug_state ();

            // always log these to "easily" spot problem in prod
            fprintf (stderr, "[Manager::download] Download: \"%s\" \"%s\"\n",
                     fname.c_str (), url.c_str ());

            namespace cc = child::command;

            const std::string qid
                = util::max_len (util::base64::encode (fname), 32);

            const std::string child_cmd
                = cc::create_arg_sanitize_value (cc::command_options_keys_t.id,
                                                 qid)
                  + cc::create_arg (cc::command_options_keys_t.command,
                                    cc::command_execute_commands_t.dl_music)
                  + cc::create_arg_sanitize_value (
                      cc::command_options_keys_t.file_path, filepath)
                  + cc::create_arg_sanitize_value (
                      cc::command_options_keys_t.ytdlp_query, url)
                  + cc::create_arg_sanitize_value (
                      cc::command_options_keys_t.ytdlp_util_exe, yt_dlp)
                  + cc::create_arg (cc::command_options_keys_t.debug,
                                    debug ? "1" : "0");

            const std::string exit_cmd = cc::get_exit_command (qid);

            // send download command then wait until it exits
            cc::send_command (child_cmd);
            int status = child::command::wait_slave_ready (qid, 10);
            if (status != 0)
                {
                    fprintf (
                        stderr,
                        "[Manager::download ERROR] Error downloading '%s' "
                        "to '%s' with code %d\n",
                        url.c_str (), filepath.c_str (), status);
                }
            else
                {
                    const std::string notif_fifo_path
                        = child::dl_music::get_download_music_fifo_path (qid);

                    int notif_fifo = open (notif_fifo_path.c_str (), O_RDONLY);

                    if (notif_fifo < 0)
                        fprintf (stderr,
                                 "[Manager::download ERROR] "
                                 "Failed to open notif_fifo: '%s'\n",
                                 notif_fifo_path.c_str ());
                    else
                        {
                            status
                                = read_notif_fifo (notif_fifo, filepath, url);
                            close (notif_fifo);
                            notif_fifo = -1;
                        }

                    cc::send_command (exit_cmd);
                }

            {
                std::lock_guard lk (this->dl_m);
                this->waiting_file_download.erase (fname);

                // update newly downloaded file access time
                bool utimeerr = false;
                struct stat downloaded_stat;
                struct utimbuf new_times;

                if (stat (filepath.c_str (), &downloaded_stat) == 0)
                    {
                        new_times.actime
                            = time (NULL); /* set atime to current time */
                        new_times.modtime
                            = downloaded_stat
                                  .st_mtime; /* keep mtime unchanged */
                        if (utime (filepath.c_str (), &new_times) < 0)
                            {
                                perror (filepath.c_str ());
                                utimeerr = true;
                            }
                    }
                else
                    {
                        perror (filepath.c_str ());
                        utimeerr = true;
                    }

                // if above access time update success
                if (!utimeerr)
                    // tells main loop to control music cache
                    set_should_check_music_cache (true);
            }

            this->dl_cv.notify_all ();

            // TODO: set status somewhere when needed?
        },
        fname, url, guild_id);

    thread_manager::dispatch (tj);
}

int
Manager::play (const dpp::snowflake &guild_id, player::MCTrack &track,
               const dpp::snowflake &channel_id)
{
    std::thread tj (
        [this, &track, guild_id] (dpp::snowflake channel_id) {
            thread_manager::DoneSetter tmds;

            bool debug = get_debug_state ();

            auto guild_player = this->get_player (guild_id);
            if (!guild_player)
                {
                    std::cerr << "[Manager::play ERROR] Guild player missing: "
                              << guild_id << "\n";
                    return;
                }

            std::lock_guard lkstream (guild_player->stream_m);

            auto *vclient = guild_player->get_voice_client ();
            if (!vclient)
                {
                    std::cerr << "[Manager::play ERROR] Voice client missing: "
                              << guild_id << "\n";
                    return;
                }

            auto voice_channel_id = vclient->channel_id;

            if (debug)
                std::cerr << "[Manager::play] Attempt to stream: " << guild_id
                          << ' ' << voice_channel_id << '\n';

            int err = 0;
            try
                {
                    if (guild_player->init_for_stream () != 0)
                        return;

                    this->stream (guild_player->guild_id, track);
                }
            catch (int e)
                {
                    err = e;

                    fprintf (stderr,
                             "[ERROR Manager::play] Stream thrown "
                             "error with "
                             "code: %d\n",
                             e);

                    const bool has_send_msg_perm
                        = guild_id && voice_channel_id
                          && has_permissions_from_ids (
                              guild_id, this->cluster->me.id, channel_id,
                              { dpp::p_view_channel, dpp::p_send_messages });

                    if (!has_send_msg_perm)
                        goto skip_send_msg;

                    string msg = "";

                    // Maybe connect/reconnect here if there's
                    // connection error
                    if (e == 2)
                        msg = "`[ERROR]` Error while streaming, can't start "
                              "playback";
                    else if (e == 1)
                        msg = "`[ERROR]` No connection";

                    if (!msg.empty ())
                        {
                            const dpp::message m (channel_id, msg);

                            this->cluster->message_create (m);
                        }
                }

        skip_send_msg:
            guild_player->done_streaming ();

            // update voice client pointer after long stream session
            vclient = guild_player->get_voice_client ();

            const bool err_processor = err == 3;
            // do not insert marker when error coming from duplicate processor
            if (!err_processor && vclient && !vclient->terminating)
                {
                    vclient->insert_marker ("e");
                    return;
                }

            if (err_processor)
                return;

            auto vcc = get_voice_from_gid (guild_id, get_sha_id ());

            if (vcc.first)
                {
                    return;
                }

            if (guild_id && voice_channel_id)
                {
                    this->set_connecting (guild_id, voice_channel_id);
                }
        },
        channel_id);

    thread_manager::dispatch (tj);

    return 0;
}

size_t
Manager::remove_track (const dpp::snowflake &guild_id, const size_t &pos,
                       const size_t &amount, const size_t &to)
{
    auto guild_player = this->get_player (guild_id);
    if (!guild_player)
        return 0;

    return guild_player->remove_track (pos, amount, to);
}

bool
Manager::shuffle_queue (const dpp::snowflake &guild_id, bool update_info_embed)
{
    auto guild_player = this->get_player (guild_id);
    if (!guild_player)
        return false;

    return guild_player->shuffle (update_info_embed);
}

} // musicat::player
