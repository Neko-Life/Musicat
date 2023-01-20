#include "musicat/cmds.h"
#include "musicat/db.h"
#include "musicat/musicat.h"
#include "musicat/player.h"

namespace musicat
{
namespace player
{
using string = std::string;

bool
Manager::handle_on_track_marker (const dpp::voice_track_marker_t &event,
                                 std::shared_ptr<Manager> shared_manager)
{
    const bool debug = get_debug_state ();
    if (debug)
        printf ("Handling voice marker: \"%s\" in guild %ld\n",
                event.track_meta.c_str (), event.voice_client->server_id);

    {
        std::lock_guard<std::mutex> lk (this->mp_m);
        auto l = vector_find (&this->manually_paused,
                              event.voice_client->server_id);
        if (l != this->manually_paused.end ())
            {
                this->manually_paused.erase (l);
            }
    }

    if (!event.voice_client)
        {
            printf ("NO CLIENT\n");
            return false;
        }
    if (this->is_disconnecting (event.voice_client->server_id))
        {
            if (debug)
                printf ("RETURN DISCONNECTING\n");
            return false;
        }
    auto p = this->get_player (event.voice_client->server_id);
    if (!p)
        {
            if (debug)
                printf ("NO PLAYER\n");
            return false;
        }

    bool just_loaded_queue = false;
    if (p->saved_queue_loaded != true)
        {
            this->load_guild_current_queue (event.voice_client->server_id,
                                            &sha_id);
            just_loaded_queue = true;
        }
    if (p->saved_config_loaded != true)
        this->load_guild_player_config (event.voice_client->server_id);

    MCTrack s;
    {
        {
            std::lock_guard<std::mutex> lk (p->q_m);
            if (p->queue.size () == 0)
                {
                    if (debug)
                        printf ("NO SIZE BEFORE: %d\n", p->loop_mode);
                    return false;
                }
        }

        // Handle shifted tracks (tracks shifted to the front of the queue)
        if (debug)
            printf ("Resetting shifted: %d\n", p->reset_shifted ());
        std::lock_guard<std::mutex> lk (p->q_m);

        // Do stuff according to loop mode when playback ends
        if (event.track_meta == "e" && !p->is_stopped ())
            {
                if (p->loop_mode == loop_mode_t::l_none)
                    p->queue.pop_front ();
                else if (p->loop_mode == loop_mode_t::l_queue)
                    {
                        auto l = p->queue.front ();
                        p->queue.pop_front ();
                        p->queue.push_back (l);
                    }
            }
        else if (event.track_meta == "rm")
            {
                p->queue.pop_front ();
                return false;
            }

        if (p->queue.size () == 0)
            {
                if (debug)
                    printf ("NO SIZE AFTER: %d\n", p->loop_mode);
                if (!just_loaded_queue)
                    database::delete_guild_current_queue (
                        event.voice_client->server_id);
                return false;
            }

        p->queue.front ().skip_vote.clear ();
        s = p->queue.front ();
        p->set_stopped (false);
        if (!just_loaded_queue)
            database::update_guild_current_queue (
                event.voice_client->server_id, p->queue);
    }

    try
        {
            auto c = get_voice_from_gid (event.voice_client->server_id,
                                         this->sha_id);
            if (!has_listener (&c.second))
                return false;
        }
    catch (...)
        {
        }

    if (event.voice_client && event.voice_client->get_secs_remaining () < 0.1)
        {
            std::thread tj (
                [this, shared_manager,
                 debug] (dpp::discord_voice_client *v, MCTrack track,
                         string meta, std::shared_ptr<Player> player) {
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
                        std::unique_lock<std::mutex> lk (this->wd_m);
                        auto a = this->waiting_vc_ready.find (guild_id);
                        if (a != this->waiting_vc_ready.end ())
                            {
                                if (debug)
                                    printf ("Waiting for ready state\n");

                                this->dl_cv.wait (lk, [this, &guild_id,
                                                       &timed_out] () {
                                    auto t = this->waiting_vc_ready.find (
                                        guild_id);
                                    // if (timed_out)
                                    // {
                                    //     this->waiting_vc_ready.erase(t);
                                    //     return true;
                                    // }
                                    auto c
                                        = t == this->waiting_vc_ready.end ();
                                    return c;
                                });
                            }
                    }

                    {
                        std::unique_lock<std::mutex> lk (this->dl_m);
                        auto a = this->waiting_file_download.find (
                            track.filename);
                        if (a != this->waiting_file_download.end ())
                            {
                                if (debug)
                                    printf ("Waiting for download\n");

                                this->dl_cv.wait (lk, [this, track,
                                                       &timed_out] () {
                                    auto t = this->waiting_file_download.find (
                                        track.filename);
                                    // if (timed_out)
                                    // {
                                    //     this->waiting_file_download.erase(t);
                                    //     return true;
                                    // }
                                    auto c = t
                                             == this->waiting_file_download
                                                    .end ();
                                    return c;
                                });
                            }
                    }

                    string id = track.id ();
                    if (player->auto_play)
                        {
                            if (debug)
                                printf ("Getting new autoplay track: %s\n",
                                        id.c_str ());
                            command::play::add_track (
                                true, v->server_id,
                                string ("https://www.youtube.com/watch?v=")
                                    + id + "&list=RD" + id,
                                0, true, NULL, 0, this->sha_id, shared_manager,
                                false, player->from);
                        }

                    {
                        std::lock_guard<std::mutex> lk (player->h_m);
                        if (player->max_history_size)
                            {
                                player->history.push_back (id);
                                while (player->history.size ()
                                       > player->max_history_size)
                                    {
                                        player->history.pop_front ();
                                    }
                            }
                    }

                    auto c = dpp::find_channel (channel_id);
                    auto g = dpp::find_guild (guild_id);
                    bool embed_perms = has_permissions (
                        g, &this->cluster->me, c,
                        { dpp::p_view_channel, dpp::p_send_messages,
                          dpp::p_embed_links });
                    {
                        const string music_folder_path
                            = get_music_folder_path ();
                        std::ifstream test (music_folder_path + track.filename,
                                            std::ios_base::in
                                                | std::ios_base::binary);
                        if (!test.is_open ())
                            {
                                if (v && !v->terminating)
                                    v->insert_marker ("e");
                                if (embed_perms)
                                    {
                                        dpp::message m;
                                        m.set_channel_id (channel_id)
                                            .set_content ("Can't play track: "
                                                          + track.title ()
                                                          + " (added by <@"
                                                          + std::to_string (
                                                              track.user_id)
                                                          + ">)");
                                        this->cluster->message_create (m);
                                    }
                                return;
                            }
                        else
                            test.close ();
                    }
                    if (timed_out)
                        throw exception ("Operation took too long, aborted...",
                                         0);
                    if (meta == "r")
                        v->send_silence (60);

                    // Send play info embed
                    try
                        {
                            this->play (v, track.filename, channel_id,
                                        embed_perms);
                            if (embed_perms)
                                {
                                    // Update if last message is the info embed
                                    // message
                                    if (c && player->info_message
                                        && c->last_message_id
                                        && c->last_message_id
                                               == player->info_message->id)
                                        {
                                            if (player->loop_mode
                                                    != loop_mode_t::l_song
                                                && player->loop_mode
                                                       != loop_mode_t::
                                                           l_song_queue)
                                                this->update_info_embed (
                                                    guild_id, true);
                                        }
                                    else
                                        {
                                            this->delete_info_embed (guild_id);
                                            this->send_info_embed (
                                                guild_id, false, true);
                                        }
                                }
                            else
                                fprintf (stderr,
                                         "[EMBED_UPDATE] No channel or "
                                         "permission to send info embed\n");
                        }
                    catch (const exception &e)
                        {
                            fprintf (stderr, "[ERROR EMBED_UPDATE] %s\n",
                                     e.what ());
                            auto cd = e.code ();
                            if (cd == 1 || cd == 2)
                                if (embed_perms)
                                    {
                                        dpp::message m;
                                        m.set_channel_id (channel_id)
                                            .set_content (e.what ());
                                        this->cluster->message_create (m);
                                    }
                        }
                },
                event.voice_client, s, event.track_meta, p);
            tj.detach ();
            return true;
        }
    else if (debug)
        printf ("RETURN NO TRACK SIZE\n");
    return false;
}

void
Manager::handle_on_voice_ready (const dpp::voice_ready_t &event)
{
    const bool debug = get_debug_state ();
    {
        std::lock_guard<std::mutex> lk (this->wd_m);
        if (debug)
            printf ("[EVENT] on_voice_ready\n");
        if (this->waiting_vc_ready.find (event.voice_client->server_id)
            != this->waiting_vc_ready.end ())
            {
                this->waiting_vc_ready.erase (event.voice_client->server_id);
                this->dl_cv.notify_all ();
            }
    }

    {
        std::lock_guard<std::mutex> lk (this->c_m);
        if (this->connecting.find (event.voice_client->server_id)
            != this->connecting.end ())
            {
                this->connecting.erase (event.voice_client->server_id);
                this->dl_cv.notify_all ();
            }
    }

    auto i = event.voice_client->get_tracks_remaining ();
    auto l = event.voice_client->get_secs_remaining ();
    if (debug)
        printf ("TO INSERT %d::%f\n", i, l);
    if (l < 0.1)
        {
            event.voice_client->insert_marker ("r");
            if (debug)
                printf ("INSERTED \"r\" MARKER\n");
        }
}

void
Manager::handle_on_voice_state_update (const dpp::voice_state_update_t &event)
{
    const bool debug = get_debug_state ();

    // Non client's user code
    if (event.state.user_id != sha_id)
        {
            dpp::voiceconn *v = event.from->get_voice (event.state.guild_id);

            if (!v || !v->channel_id || !v->voiceclient)
                return;

            // Pause audio when no user listening in vc
            if (v->channel_id != event.state.channel_id
                && v->voiceclient->get_secs_remaining () > 0.1
                && !v->voiceclient->is_paused ())
                {
                    std::lock_guard<std::mutex> lk (this->mp_m);
                    if (event.from
                        && vector_find (&this->manually_paused,
                                        event.state.guild_id)
                               == this->manually_paused.end ())
                        {
                            try
                                {
                                    auto voice = get_voice_from_gid (
                                        event.state.guild_id, sha_id);
                                    // Whether there's human listening in the
                                    // vc
                                    bool p = false;
                                    for (auto l : voice.second)
                                        {
                                            // This only check user in cache,
                                            // if user not in cache then skip
                                            auto a = dpp::find_user (l.first);
                                            if (a)
                                                {
                                                    if (!a->is_bot ())
                                                        {
                                                            p = true;
                                                            break;
                                                        }
                                                }
                                        }
                                    if (!p)
                                        {
                                            v->voiceclient->pause_audio (true);
                                            this->update_info_embed (
                                                event.state.guild_id);
                                            if (debug)
                                                printf ("Paused %ld as no "
                                                        "user in vc\n",
                                                        event.state.guild_id);
                                        }
                                }
                            catch (const char *e)
                                {
                                    fprintf (stderr,
                                             "[ERROR VOICE_STATE_UPDATE] %s\n",
                                             e);
                                }
                        }
                }
            else
                {
                    if (v->channel_id == event.state.channel_id
                        && v->voiceclient->get_secs_remaining () > 0.1
                        && v->voiceclient->is_paused ())
                        {
                            std::lock_guard<std::mutex> lk (this->mp_m);
                            // Whether the track paused automatically
                            if (vector_find (&this->manually_paused,
                                             event.state.guild_id)
                                == this->manually_paused.end ())
                                {
                                    std::thread tj (
                                        [this, event] (
                                            dpp::discord_voice_client *vc) {
                                            std::this_thread::sleep_for (
                                                std::chrono::milliseconds (
                                                    2500));
                                            try
                                                {
                                                    auto a
                                                        = get_voice_from_gid (
                                                            event.state
                                                                .guild_id,
                                                            event.state
                                                                .user_id);
                                                    if (vc && !vc->terminating
                                                        && a.first
                                                        && a.first->id
                                                               == event.state
                                                                      .channel_id)
                                                        {
                                                            vc->pause_audio (
                                                                false);
                                                            this->update_info_embed (
                                                                event.state
                                                                    .guild_id);
                                                        }
                                                }
                                            catch (...)
                                                {
                                                }
                                        },
                                        v->voiceclient);
                                    tj.detach ();
                                }
                        }
                    else if (event.state.channel_id
                             && v->channel_id != event.state.channel_id)
                        {
                            try
                                {
                                    auto m = get_voice_from_gid (
                                        event.state.guild_id, this->sha_id);
                                    if (has_listener (&m.second))
                                        {
                                            std::lock_guard<std::mutex> lk (
                                                this->dc_m);
                                            std::lock_guard<std::mutex> lk2 (
                                                this->c_m);
                                            std::lock_guard<std::mutex> lk3 (
                                                this->wd_m);
                                            this->disconnecting[event.state
                                                                    .guild_id]
                                                = v->channel_id;
                                            this->connecting[event.state
                                                                 .guild_id]
                                                = event.state.channel_id;
                                            this->waiting_vc_ready
                                                [event.state.guild_id]
                                                = "0";
                                            event.from->disconnect_voice (
                                                event.state.guild_id);
                                        }
                                    std::thread tj ([this, event] () {
                                        this->reconnect (event.from,
                                                         event.state.guild_id);
                                    });
                                    tj.detach ();
                                }
                            catch (...)
                                {
                                }
                        }
                }

            // End non client's user code
            return;
        }

    // Client user code -----------------------------------

    if (!event.state.channel_id)
        {
            std::lock_guard<std::mutex> lk (this->dc_m);
            if (debug)
                printf ("[EVENT] on_voice_state_leave\n");
            if (this->disconnecting.find (event.state.guild_id)
                != this->disconnecting.end ())
                {
                    this->disconnecting.erase (event.state.guild_id);
                    this->dl_cv.notify_all ();
                }
        }
    else
        {
            {
                std::lock_guard<std::mutex> lk (this->c_m);
                if (debug)
                    printf ("[EVENT] on_voice_state_join\n");
                if (this->connecting.find (event.state.guild_id)
                    != this->connecting.end ())
                    {
                        this->connecting.erase (event.state.guild_id);
                        this->dl_cv.notify_all ();
                    }
            }

            try
                {
                    auto a = get_voice_from_gid (event.state.guild_id,
                                                 event.state.user_id);
                    auto v = event.from->get_voice (event.state.guild_id);

                    if (v && v->voiceclient)
                        {
                            std::lock_guard<std::mutex> lk (this->wd_m);
                            auto i = this->waiting_vc_ready.find (
                                v->voiceclient->server_id);
                            if (v->voiceclient->is_ready ()
                                && i != this->waiting_vc_ready.end ())
                                {
                                    this->waiting_vc_ready.erase (i);
                                    this->dl_cv.notify_all ();
                                }
                        }

                    if (v && v->channel_id && v->channel_id != a.first->id)
                        {
                            this->stop_stream (event.state.guild_id);

                            if (v->voiceclient && !v->voiceclient->terminating)
                                {
                                    v->voiceclient->pause_audio (false);
                                    v->voiceclient->skip_to_next_marker ();
                                }

                            {
                                std::lock_guard<std::mutex> lk (this->dc_m);
                                this->disconnecting[event.state.guild_id]
                                    = v->channel_id;
                                event.from->disconnect_voice (
                                    event.state.guild_id);
                            }

                            if (has_listener (&a.second))
                                {
                                    std::lock_guard<std::mutex> lk (this->c_m);
                                    std::lock_guard<std::mutex> lk2 (
                                        this->wd_m);
                                    this->connecting.insert_or_assign (
                                        event.state.guild_id, a.first->id);
                                    this->waiting_vc_ready[event.state
                                                               .guild_id]
                                        = "0";
                                }

                            std::thread tj ([this, event] () {
                                std::this_thread::sleep_for (
                                    std::chrono::seconds (1));
                                this->voice_ready (event.state.guild_id,
                                                   event.from);
                            });
                            tj.detach ();
                        }
                }
            catch (...)
                {
                }
        }
    // if (muted) player_manager->pause(event.guild_id);
    // else player_manager->resume(guild_id);
}

void
Manager::handle_on_message_delete (const dpp::message_delete_t &event)
{
    this->set_info_message_as_deleted (event.deleted->id);
}

void
Manager::handle_on_message_delete_bulk (
    const dpp::message_delete_bulk_t &event)
{
    for (auto i : event.deleted)
        this->set_info_message_as_deleted (i);
}

} // player
} // musicat
