#include "musicat/cmds.h"
#include "musicat/cmds/play.h"
#include "musicat/db.h"
#include "musicat/musicat.h"
#include "musicat/player.h"
#include "musicat/thread_manager.h"
#include <dirent.h>
#include <libpq-fe.h>

namespace musicat
{
namespace player
{
using string = std::string;

bool
Manager::is_disconnecting (const dpp::snowflake &guild_id)
{
    std::lock_guard<std::mutex> lk1 (this->dc_m);
    return this->disconnecting.find (guild_id) != this->disconnecting.end ();
}

void
Manager::set_disconnecting (const dpp::snowflake &guild_id,
                            const dpp::snowflake &voice_channel_id)
{
    std::lock_guard<std::mutex> lk (this->dc_m);

    this->disconnecting.insert_or_assign (guild_id, voice_channel_id);
}

void
Manager::clear_disconnecting (const dpp::snowflake &guild_id)
{
    if (get_debug_state ())
        std::cerr << "[EVENT] on_voice_state_leave: " << guild_id << '\n';

    std::lock_guard<std::mutex> lk (this->dc_m);

    auto i = this->disconnecting.find (guild_id);

    if (i != this->disconnecting.end ())
        {
            this->disconnecting.erase (i);
            this->dl_cv.notify_all ();
        }
}

bool
Manager::is_connecting (const dpp::snowflake &guild_id)
{
    std::lock_guard<std::mutex> lk2 (this->c_m);
    return this->connecting.find (guild_id) != this->connecting.end ();
}

void
Manager::set_connecting (const dpp::snowflake &guild_id,
                         const dpp::snowflake &voice_channel_id)
{
    std::lock_guard<std::mutex> lk (this->c_m);

    this->connecting.insert_or_assign (guild_id, voice_channel_id);
}

bool
Manager::is_waiting_vc_ready (const dpp::snowflake &guild_id)
{
    std::lock_guard<std::mutex> lk3 (this->wd_m);
    return this->waiting_vc_ready.find (guild_id)
           != this->waiting_vc_ready.end ();
}

void
Manager::set_waiting_vc_ready (const dpp::snowflake &guild_id,
                               const std::string &second)
{
    std::lock_guard<std::mutex> lk2 (this->wd_m);

    this->waiting_vc_ready.insert_or_assign (guild_id, second);

    this->set_vc_ready_timeout (guild_id);
}

void
Manager::set_vc_ready_timeout (const dpp::snowflake &guild_id,
                               const unsigned long &timer)
{
    std::thread t ([this, guild_id, timer] () {
        thread_manager::DoneSetter tmds;

        std::this_thread::sleep_for (std::chrono::milliseconds (timer));

        const int status = this->clear_wait_vc_ready (guild_id);

        if (status == 0)
            return;

        fprintf (stderr, "[Manager::set_vc_ready_timeout "
                         "WARN] Connection timeout\n");

        auto player_manager = get_player_manager_ptr ();

        auto guild_player
            = player_manager ? player_manager->get_player (guild_id) : nullptr;

        dpp::snowflake channel_id
            = guild_player ? guild_player->channel_id : dpp::snowflake (0);

        const auto sha_id = get_sha_id ();

        std::pair<dpp::channel *, std::map<dpp::snowflake, dpp::voicestate> >
            vcs;

        if (!player_manager || !guild_player || !guild_player->from)
            goto skip_disconnecting;

        vcs = get_voice_from_gid (guild_id, sha_id);

        if (!vcs.first || !vcs.first->id)
            goto skip_disconnecting;

        player_manager->set_disconnecting (guild_id, vcs.first->id);

        guild_player->from->disconnect_voice (guild_id);

        // this jump means there's no need to disconnect
    skip_disconnecting:

        if (!channel_id)
            {
                return;
            }

        auto server_id = guild_player->guild_id;

        bool has_send_msg_perm
            = server_id
              && has_permissions_from_ids (
                  server_id, this->cluster->me.id, channel_id,
                  { dpp::p_view_channel, dpp::p_send_messages });

        if (!has_send_msg_perm)
            return;

        dpp::message m ("Seems like the voice "
                        "server isn't "
                        "responding, try "
                        "changing your voice "
                        "region in the voice "
                        "channel setting");

        m.set_channel_id (channel_id);

        this->cluster->message_create (m);
    });

    thread_manager::dispatch (t);
}

void
Manager::wait_for_vc_ready (const dpp::snowflake &guild_id)
{

    if (!is_waiting_vc_ready (guild_id))
        return;

    if (get_debug_state ())
        std::cerr << "[Manager::wait_for_vc_ready] Waiting for ready state: "
                  << guild_id << '\n';

    std::unique_lock<std::mutex> lk (this->wd_m);
    this->dl_cv.wait (lk, [this, &guild_id] () {
        return this->waiting_vc_ready.find (guild_id)
               == this->waiting_vc_ready.end ();
    });
}

int
Manager::clear_wait_vc_ready (const dpp::snowflake &guild_id)
{
    if (get_debug_state ())
        std::cerr << "[Manager::clear_wait_vc_ready]: " << guild_id << '\n';

    int err = this->clear_connecting (guild_id);

    std::lock_guard<std::mutex> lk (this->wd_m);

    auto i = this->waiting_vc_ready.find (guild_id);

    if (i != this->waiting_vc_ready.end ())
        {
            this->waiting_vc_ready.erase (i);
            this->dl_cv.notify_all ();
            return 2;
        }

    return err;
}

int
Manager::clear_connecting (const dpp::snowflake &guild_id)
{
    if (get_debug_state ())
        std::cerr << "[Manager::clear_connecting]: " << guild_id << '\n';

    std::lock_guard<std::mutex> lk (this->c_m);

    auto i = this->connecting.find (guild_id);

    if (i != this->connecting.end ())
        {
            this->connecting.erase (i);
            this->dl_cv.notify_all ();
            return 1;
        }

    return 0;
}

bool
Manager::is_manually_paused (const dpp::snowflake &guild_id)
{
    std::lock_guard<std::mutex> lk (this->mp_m);

    return vector_find (&this->manually_paused, guild_id)
           != this->manually_paused.end ();
}

void
Manager::set_manually_paused (const dpp::snowflake &guild_id)
{
    std::lock_guard<std::mutex> lk (this->mp_m);

    if (vector_find (&this->manually_paused, guild_id)
        == this->manually_paused.end ())
        {
            this->manually_paused.push_back (guild_id);
        }
}

void
Manager::clear_manually_paused (const dpp::snowflake &guild_id)
{
    std::lock_guard<std::mutex> lk (this->mp_m);

    auto i = vector_find (&this->manually_paused, guild_id);

    if (i != this->manually_paused.end ())
        {
            this->manually_paused.erase (i);
        }
}

bool
Manager::voice_ready (const dpp::snowflake &guild_id,
                      dpp::discord_client *from, const dpp::snowflake &user_id)
{
    bool re = is_connecting (guild_id);

    if (!is_disconnecting (guild_id) && !re && !is_waiting_vc_ready (guild_id))
        return true;

    if (!re || !from)
        return false;

    std::thread t (
        [this, user_id, guild_id] (dpp::discord_client *from) {
            thread_manager::DoneSetter tmds;

            std::pair<dpp::channel *,
                      std::map<dpp::snowflake, dpp::voicestate> >
                uservc;

            uservc = get_voice_from_gid (guild_id, user_id);

            bool user_vc = uservc.first != nullptr;
            auto f = from->connecting_voice_channels.find (guild_id);
            auto c = get_voice_from_gid (guild_id, from->creator->me.id);

            if (!c.first)
                goto reset_vc;

            if (f == from->connecting_voice_channels.end () || !f->second)
                {
                    this->set_disconnecting (guild_id, 1);

                    from->disconnect_voice (guild_id);
                }
            else if (user_vc && uservc.first->id != c.first->id)
                {
                    if (get_debug_state ())
                        std::cerr << "Disconnecting as it "
                                     "seems I just got moved "
                                     "to different vc and "
                                     "connection not updated "
                                     "yet: "
                                  << guild_id << '\n';

                    this->set_disconnecting (guild_id, f->second->channel_id);

                    this->set_connecting (guild_id, uservc.first->id);

                    from->disconnect_voice (guild_id);
                }

            goto reconnect;

        reset_vc:
            reset_voice_channel (from, guild_id);

            if (user_id && user_vc)
                {
                    std::lock_guard<std::mutex> lk (this->c_m);
                    auto p = this->connecting.find (guild_id);

                    std::map<dpp::snowflake, dpp::voicestate> vm = {};

                    if (p == this->connecting.end ())
                        goto reconnect;

                    auto gc = dpp::find_channel (p->second);
                    if (gc)
                        vm = gc->get_voice_members ();

                    auto l = has_listener (&vm);
                    if (!l && p->second != uservc.first->id)
                        p->second = uservc.first->id;
                }
            // goto reconnect;

        reconnect:
            this->reconnect (from, guild_id);
        },
        from);

    thread_manager::dispatch (t);

    return true;
}

void
Manager::stop_stream (const dpp::snowflake &guild_id)
{
    auto vs = get_voice_from_gid (guild_id, get_sha_id ());
    if (!vs.first)
        return;

    auto guild_player = this->get_player (guild_id);

    if (!guild_player)
        return;

    guild_player->current_track.stopping = true;
    set_stream_stopping (guild_id);
}

bool
Manager::is_waiting_file_download (const string &file_name)
{
    return this->waiting_file_download.find (file_name)
           != this->waiting_file_download.end ();
}

void
Manager::wait_for_download (const string &file_name)
{
    if (!this->is_waiting_file_download (file_name))
        return;

    std::unique_lock<std::mutex> lk (this->dl_m);

    this->dl_cv.wait (lk, [this, file_name] () {
        return this->waiting_file_download.find (file_name)
               == this->waiting_file_download.end ();
    });
}

std::vector<std::string>
Manager::get_available_tracks (const size_t &amount) const
{
    std::vector<std::string> ret = {};
    size_t c = 0;
    auto dir = opendir (get_music_folder_path ().c_str ());

    if (dir != NULL)
        {
            auto file = readdir (dir);
            while (file != NULL)
                {
                    if (file->d_type == DT_REG)
                        {
                            string s = string (file->d_name);
                            ret.push_back (s.substr (0, s.length () - 5));
                        }

                    if (amount && ++c == amount)
                        break;
                    file = readdir (dir);
                }
            closedir (dir);
        }
    return ret;
}

bool
Manager::set_info_message_as_deleted (dpp::snowflake id)
{
    std::lock_guard<std::mutex> lk (this->imc_m);
    auto m = this->info_messages_cache.find (id);
    if (m != this->info_messages_cache.end ())
        {
            if (!m->second->is_source_message_deleted ())
                {
                    m->second->set_flags (m->second->flags
                                          | dpp::m_source_message_deleted);
                    return true;
                }
        }
    return false;
}

void
Manager::set_ignore_marker (const dpp::snowflake &guild_id)
{
    std::lock_guard<std::mutex> lk (this->im_m);
    auto l = vector_find (&this->ignore_marker, guild_id);
    if (l == this->ignore_marker.end ())
        {
            this->ignore_marker.push_back (guild_id);
        }
}

void
Manager::remove_ignore_marker (const dpp::snowflake &guild_id)
{
    std::lock_guard<std::mutex> lk (this->im_m);

    auto i = vector_find (&this->ignore_marker, guild_id);

    if (i != this->ignore_marker.end ())
        {
            this->ignore_marker.erase (i);
        }
}

bool
Manager::has_ignore_marker (const dpp::snowflake &guild_id)
{
    std::lock_guard<std::mutex> lk (this->im_m);
    auto l = vector_find (&this->ignore_marker, guild_id);
    if (l != this->ignore_marker.end ())
        return true;
    else
        return false;
}

int
Manager::load_guild_current_queue (const dpp::snowflake &guild_id,
                                   const dpp::snowflake *user_id)
{
    auto player = this->create_player (guild_id);

    if (player->saved_queue_loaded == true)
        return 0;

    // don't do anything if no db connected
    if (database::get_conn_status () != CONNECTION_OK)
        return -1;

    player->saved_queue_loaded = true;

    std::pair<PGresult *, ExecStatusType> res
        = database::get_guild_current_queue (guild_id);

    std::pair<std::deque<MCTrack>, int> queue
        = database::get_playlist_from_PGresult (res.first);

    database::finish_res (res.first);
    res.first = nullptr;

    if (queue.second != 0)
        return queue.second;

    for (auto &t : queue.first)
        {
            if (user_id)
                t.user_id = *user_id;

            player->add_track (t);
        }

    return queue.second;
}

int
Manager::load_guild_player_config (const dpp::snowflake &guild_id)
{
    auto player = this->create_player (guild_id);
    if (player->saved_config_loaded == true)
        return 0;

    player->saved_config_loaded = true;

    std::pair<PGresult *, ExecStatusType> res
        = database::get_guild_player_config (guild_id);

    std::pair<database::player_config, int> conf
        = database::parse_guild_player_config_PGresult (res.first);

    database::finish_res (res.first);
    res.first = nullptr;

    if (conf.second != 0)
        return conf.second;

    player->loop_mode = conf.first.loop_mode;
    player->max_history_size = (size_t)conf.first.autoplay_threshold;
    player->auto_play = conf.first.autoplay_state;

    return conf.second;
}

int
Manager::set_reconnect (const dpp::snowflake &guild_id,
                        const dpp::snowflake &disconnect_channel_id,
                        const dpp::snowflake &connect_channel_id)
{
    if (!guild_id)
        // guild_id is 0
        return -1;

    if (disconnect_channel_id)
        this->set_disconnecting (guild_id, disconnect_channel_id);

    if (connect_channel_id)
        {
            this->set_connecting (guild_id, connect_channel_id);
            this->set_waiting_vc_ready (guild_id);

            // success
            return 0;
        }

    // connect_channel_id is 0
    return 1;
}

int
Manager::full_reconnect (dpp::discord_client *from,
                         const dpp::snowflake &guild_id,
                         const dpp::snowflake &disconnect_channel_id,
                         const dpp::snowflake &connect_channel_id,
                         const bool &for_listener)
{
    const auto sha_id = get_sha_id ();

    if (for_listener)
        {
            auto m = get_voice_from_gid (guild_id, sha_id);

            if (!m.first || !has_listener (&m.second))
                return 0;
        }

    int status = this->set_reconnect (guild_id, disconnect_channel_id,
                                      connect_channel_id);

    from->disconnect_voice (guild_id);

    std::thread pjt ([this, from, guild_id] () {
        thread_manager::DoneSetter tmds;

        this->reconnect (from, guild_id);
    });

    thread_manager::dispatch (pjt);

    return status;
}

void
Manager::get_next_autoplay_track (const string &track_id,
                                  dpp::discord_client *from,
                                  const dpp::snowflake &server_id)
{
    const bool debug = get_debug_state ();

    if (debug)
        fprintf (stderr,
                 "[Manager::handle_on_track_marker] Getting new autoplay "
                 "track: %s\n",
                 track_id.c_str ());

    const string query = "https://www.youtube.com/watch?v=" + track_id
                         + "&list=RD" + track_id;

    command::play::add_track (
        true, server_id, query, 0, true, NULL, 0, get_sha_id (), false, from,
        dpp::interaction_create_t (NULL, "{}"), false, 0, track_id);
}

int
Manager::set_autopause (dpp::voiceconn *v, const dpp::snowflake &guild_id,
                        bool check_listening_user)
{
    if (!v || !v->voiceclient)
        return 1;

    if (this->is_manually_paused (guild_id))
        return -1;

    std::pair<dpp::channel *, std::map<dpp::snowflake, dpp::voicestate> > voice
        = { nullptr, {} };

    if (!check_listening_user)
        goto exec_pause_audio;

    voice = get_voice_from_gid (guild_id, get_sha_id ());

    // check whether there's human listening in the vc
    if (!voice.first)
        goto exec_pause_audio;

    for (const auto &l : voice.second)
        {
            // This only check user in cache,
            auto a = dpp::find_user (l.first);

            // if user not in cache then skip
            if (!a)
                continue;

            // don't count bot as listener
            if (a->is_bot ())
                continue;

            // has listening user, abort pause
            return -1;
        }

exec_pause_audio:
    v->voiceclient->pause_audio (true);
    this->update_info_embed (guild_id);
    return 0;
}

} // player
} // musicat
