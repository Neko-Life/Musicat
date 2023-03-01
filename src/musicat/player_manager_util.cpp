#include "musicat/db.h"
#include "musicat/musicat.h"
#include "musicat/player.h"
#include <dirent.h>

namespace musicat
{
namespace player
{
using string = std::string;

bool
Manager::is_disconnecting (dpp::snowflake guild_id)
{
    std::lock_guard<std::mutex> lk1 (this->dc_m);
    return this->disconnecting.find (guild_id) != this->disconnecting.end ();
}

bool
Manager::is_connecting (dpp::snowflake guild_id)
{
    std::lock_guard<std::mutex> lk2 (this->c_m);
    return this->connecting.find (guild_id) != this->connecting.end ();
}

bool
Manager::is_waiting_vc_ready (dpp::snowflake guild_id)
{
    std::lock_guard<std::mutex> lk3 (this->wd_m);
    return this->waiting_vc_ready.find (guild_id)
           != this->waiting_vc_ready.end ();
}

bool
Manager::voice_ready (dpp::snowflake guild_id, dpp::discord_client *from,
                      dpp::snowflake user_id)
{
    bool re = is_connecting (guild_id);
    if (is_disconnecting (guild_id) || re || is_waiting_vc_ready (guild_id))
        {
            if (re && from)
                {
                    std::thread t (
                        [this, user_id, guild_id] (dpp::discord_client *from) {
                            bool user_vc = true;
                            std::pair<
                                dpp::channel *,
                                std::map<dpp::snowflake, dpp::voicestate> >
                                uservc;

                            try
                                {
                                    uservc = get_voice_from_gid (guild_id,
                                                                 user_id);
                                }
                            catch (...)
                                {
                                    user_vc = false;
                                }

                            try
                                {
                                    auto f = from->connecting_voice_channels
                                                 .find (guild_id);
                                    auto c = get_voice_from_gid (
                                        guild_id, from->creator->me.id);
                                    if (f
                                            == from->connecting_voice_channels
                                                   .end ()
                                        || !f->second)
                                        {
                                            std::lock_guard<std::mutex> lk (
                                                this->dc_m);
                                            this->disconnecting[guild_id] = 1;
                                            from->disconnect_voice (guild_id);
                                        }
                                    else if (user_vc
                                             && uservc.first->id
                                                    != c.first->id)
                                        {
                                            if (get_debug_state ())
                                                printf (
                                                    "Disconnecting as it "
                                                    "seems I just got moved "
                                                    "to different vc and "
                                                    "connection not updated "
                                                    "yet: %ld\n",
                                                    guild_id);
                                            std::lock_guard<std::mutex> lk (
                                                this->dc_m);
                                            std::lock_guard<std::mutex> lk2 (
                                                this->c_m);
                                            this->disconnecting[guild_id]
                                                = f->second->channel_id;
                                            this->connecting[guild_id]
                                                = uservc.first->id;
                                            from->disconnect_voice (guild_id);
                                        }
                                }
                            catch (...)
                                {
                                    reset_voice_channel (from, guild_id);

                                    if (user_id && user_vc)
                                        try
                                            {
                                                std::lock_guard<std::mutex>
                                                    lk (this->c_m);
                                                auto p
                                                    = this->connecting.find (
                                                        guild_id);
                                                std::map<dpp::snowflake,
                                                         dpp::voicestate>
                                                    vm = {};
                                                if (p
                                                    != this->connecting.end ())
                                                    {
                                                        auto gc = dpp::
                                                            find_channel (
                                                                p->second);
                                                        if (gc)
                                                            vm = gc->get_voice_members ();
                                                        auto l = has_listener (
                                                            &vm);
                                                        if (!l
                                                            && p->second
                                                                   != uservc
                                                                          .first
                                                                          ->id)
                                                            p->second
                                                                = uservc.first
                                                                      ->id;
                                                    }
                                            }
                                        catch (...)
                                            {
                                            }
                                }
                            this->reconnect (from, guild_id);
                        },
                        from);
                    t.detach ();
                }
            return false;
        }
    return true;
}

void
Manager::stop_stream (dpp::snowflake guild_id)
{
    auto guild_player = this->get_player (guild_id);
    if (guild_player)
        if (guild_player->current_track.filesize)
            guild_player->current_track.stopping = true;
}

void
Manager::wait_for_download (string file_name)
{
    std::unique_lock<std::mutex> lk (this->dl_m);
    this->dl_cv.wait (lk, [this, file_name] () {
        return this->waiting_file_download.find (file_name)
               == this->waiting_file_download.end ();
    });
}

std::vector<std::string>
Manager::get_available_tracks (const size_t amount) const
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
    auto l = vector_find (&this->ignore_marker, guild_id);
    if (l != this->ignore_marker.end ())
        {
            this->ignore_marker.erase (l);
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

} // player
} // musicat
