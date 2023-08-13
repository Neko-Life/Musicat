#include "channel.h"
#include "musicat/cmds.h"
#include "musicat/db.h"
#include "musicat/musicat.h"
#include "musicat/player.h"
#include "musicat/thread_manager.h"

namespace musicat
{
namespace player
{
// this section can't be anymore horrible than this....
using string = std::string;

bool
Manager::handle_on_track_marker (const dpp::voice_track_marker_t &event,
                                 std::shared_ptr<Manager> shared_manager)
{
    const bool debug = get_debug_state ();

    if (!event.voice_client)
        {
            fprintf (stderr, "NO CLIENT\n");
            return false;
        }

    if (debug)
        fprintf (stderr, "Handling voice marker: \"%s\" in guild %ld\n",
                 event.track_meta.c_str (), event.voice_client->server_id);

    shared_manager->clear_manually_paused (event.voice_client->server_id);

    if (this->is_disconnecting (event.voice_client->server_id))
        {
            if (debug)
                fprintf (stderr, "RETURN DISCONNECTING\n");

            return false;
        }

    auto guild_player = this->get_player (event.voice_client->server_id);

    if (!guild_player)
        {
            if (debug)
                fprintf (stderr, "NO PLAYER\n");

            return false;
        }

    clear_stream_stopping (event.voice_client->server_id);

    if (debug)
        fprintf (
            stderr,
            "[Manager::handle_on_track_marker] Locked player::t_mutex: %ld\n",
            guild_player->guild_id);

    std::lock_guard<std::mutex> lk (guild_player->t_mutex);

    bool just_loaded_queue = false;
    if (guild_player->saved_queue_loaded != true)
        {
            this->load_guild_current_queue (event.voice_client->server_id,
                                            &sha_id);
            just_loaded_queue = true;
        }

    if (guild_player->saved_config_loaded != true)
        this->load_guild_player_config (event.voice_client->server_id);

    if (guild_player->queue.size () == 0)
        {
            if (debug)
                {
                    fprintf (stderr, "NO SIZE BEFORE: %d\n",
                             guild_player->loop_mode);
                    fprintf (stderr,
                             "[Manager::handle_on_track_marker] Should unlock "
                             "player::t_mutex: %ld\n",
                             guild_player->guild_id);
                }

            return false;
        }

    // Handle shifted tracks (tracks shifted to the front of the queue)
    if (debug)
        fprintf (stderr, "Resetting shifted: %d\n",
                 guild_player->reset_shifted ());

    // Do stuff according to loop mode when playback ends
    if (event.track_meta == "e" && !guild_player->is_stopped ())
        {
            switch (guild_player->loop_mode)
                {
                case loop_mode_t::l_none:
                    {
                        guild_player->queue.pop_front ();
                        break;
                    }

                case loop_mode_t::l_queue:
                    {
                        auto l = guild_player->queue.front ();
                        guild_player->queue.pop_front ();
                        guild_player->queue.push_back (l);
                        break;
                    }

                default:
                    break;
                }
        }
    else if (event.track_meta == "rm")
        {
            guild_player->queue.pop_front ();
            if (debug)
                fprintf (stderr,
                         "[Manager::handle_on_track_marker] Should unlock "
                         "player::t_mutex: %ld\n",
                         guild_player->guild_id);

            return false;
        }

    if (guild_player->queue.size () == 0)
        {
            if (debug)
                {
                    fprintf (stderr, "NO SIZE AFTER: %d\n",
                             guild_player->loop_mode);
                    fprintf (stderr,
                             "[Manager::handle_on_track_marker] Should unlock "
                             "player::t_mutex: %ld\n",
                             guild_player->guild_id);
                }

            if (!just_loaded_queue)
                database::delete_guild_current_queue (
                    event.voice_client->server_id);

            return false;
        }

    guild_player->queue.front ().skip_vote.clear ();
    guild_player->queue.front ().seekable = false;

    guild_player->current_track = guild_player->queue.front ();
    guild_player->queue.front ().seek_to = -1;

    guild_player->set_stopped (false);

    if (!just_loaded_queue)
        database::update_guild_current_queue (event.voice_client->server_id,
                                              guild_player->queue);

    try
        {
            auto c = get_voice_from_gid (event.voice_client->server_id,
                                         this->sha_id);
            if (!has_listener (&c.second))
                {
                    if (debug)
                        fprintf (stderr,
                                 "[Manager::handle_on_track_marker] Should "
                                 "unlock player::t_mutex: %ld\n",
                                 guild_player->guild_id);

                    return false;
                }
        }
    catch (...)
        {

            return false;
        }

    if (event.voice_client
        && event.voice_client->get_secs_remaining () < 0.05f)
        {
            std::thread tj (
                [this, shared_manager,
                 debug] (dpp::discord_voice_client *v, string meta,
                         std::shared_ptr<Player> guild_player) {
                    try
                        {
                            MCTrack &track = guild_player->current_track;
                            auto guild_id = v->server_id;

                            std::lock_guard<std::mutex> lk (
                                guild_player->t_mutex);
                            // text channel to send embed
                            dpp::snowflake channel_id
                                = guild_player->channel_id;

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

                            this->wait_for_vc_ready (guild_id);

                            // channel for sending message
                            dpp::channel *c = dpp::find_channel (channel_id);
                            // guild
                            dpp::guild *g = dpp::find_guild (guild_id);

                            prepare_play_stage_channel_routine (v, g);

                            this->wait_for_download (track.filename);

                            string id = track.id ();
                            if (guild_player->auto_play)
                                {
                                    std::thread at_t ([debug, id, this,
                                                       shared_manager,
                                                       guild_player, v] () {
                                        try
                                            {
                                                if (debug)
                                                    fprintf (
                                                        stderr,
                                                        "Getting new autoplay "
                                                        "track: %s\n",
                                                        id.c_str ());

                                                command::play::add_track (
                                                    true, v->server_id,
                                                    string ("https://"
                                                            "www.youtube.com/"
                                                            "watch?v=")
                                                        + id + "&list=RD" + id,
                                                    0, true, NULL, 0,
                                                    this->sha_id,
                                                    shared_manager, false,
                                                    guild_player->from);
                                            }
                                        catch (...)
                                            {
                                            }

                                        thread_manager::set_done ();
                                    });

                                    thread_manager::dispatch (at_t);
                                }

                            if (guild_player->max_history_size)
                                {
                                    guild_player->history.push_back (id);

                                    while (guild_player->history.size ()
                                           > guild_player->max_history_size)
                                        {
                                            guild_player->history.pop_front ();
                                        }
                                }

                            bool embed_perms = has_permissions (
                                g, &this->cluster->me, c,
                                { dpp::p_view_channel, dpp::p_send_messages,
                                  dpp::p_embed_links });

                            {
                                const string music_folder_path
                                    = get_music_folder_path ();

                                std::ifstream test (
                                    music_folder_path + track.filename,
                                    std::ios_base::in | std::ios_base::binary);
                                if (!test.is_open ())
                                    {
                                        if (v && !v->terminating)
                                            {
                                                this->remove_ignore_marker (
                                                    guild_id);
                                                v->insert_marker ("e");
                                            }

                                        if (embed_perms)
                                            {
                                                dpp::message m;
                                                m.set_channel_id (channel_id)
                                                    .set_content (
                                                        "Can't play track: "
                                                        + track.title ()
                                                        + " (added by <@"
                                                        + std::to_string (
                                                            track.user_id)
                                                        + ">)");

                                                this->cluster->message_create (
                                                    m);
                                            }

                                        if (debug)
                                            fprintf (stderr,
                                                     "[thread tj "
                                                     "Manager::handle_on_"
                                                     "track_marker] "
                                                     "Should unlock "
                                                     "player::t_mutex: %ld\n",
                                                     guild_player->guild_id);

                                        thread_manager::set_done ();
                                        return;
                                    }
                                else
                                    test.close ();
                            }

                            if (meta == "r")
                                v->send_silence (60);

                            // Send play info embed
                            try
                                {
                                    this->play (v, track, channel_id);

                                    if (embed_perms)
                                        {
                                            // Update if last message is the
                                            // info embed message
                                            const bool should_update_embed
                                                = c
                                                  && guild_player->info_message
                                                  && c->last_message_id
                                                  && c->last_message_id
                                                         == guild_player
                                                                ->info_message
                                                                ->id;

                                            if (should_update_embed)
                                                {
                                                    const bool
                                                        not_repeating_song
                                                        = (guild_player
                                                               ->loop_mode
                                                           != loop_mode_t::
                                                               l_song)
                                                          && (guild_player
                                                                  ->loop_mode
                                                              != loop_mode_t::
                                                                  l_song_queue);

                                                    if (not_repeating_song)
                                                        this->update_info_embed (
                                                            guild_id, true);
                                                }
                                            else
                                                {
                                                    this->delete_info_embed (
                                                        guild_id);

                                                    this->send_info_embed (
                                                        guild_id, false, true);
                                                }
                                        }
                                    else
                                        fprintf (
                                            stderr,
                                            "[EMBED_UPDATE] No channel or "
                                            "permission to send info embed\n");
                                }
                            catch (const exception &e)
                                {
                                    fprintf (stderr,
                                             "[ERROR EMBED_UPDATE] %s\n",
                                             e.what ());

                                    auto cd = e.code ();

                                    if (!embed_perms || (cd != 1 && cd != 2))
                                        {
                                            thread_manager::set_done ();
                                            return;
                                        }

                                    dpp::message m;
                                    m.set_channel_id (channel_id)
                                        .set_content (e.what ());

                                    this->cluster->message_create (m);
                                }
                        }
                    catch (...)
                        {
                        }

                    thread_manager::set_done ();
                },
                event.voice_client, event.track_meta, guild_player);

            thread_manager::dispatch (tj);

            if (debug)
                fprintf (stderr,
                         "[Manager::handle_on_track_marker] Should unlock "
                         "player::t_mutex: %ld\n",
                         guild_player->guild_id);

            return true;
        }
    else
        fprintf (stderr, "[Manager::handle_on_track_marker WARN] Voice "
                         "client not present or already playing: %ld\n",
                         guild_player->guild_id);

    if (debug)
        {
            fprintf (stderr,
                     "[Manager::handle_on_track_marker] Should unlock "
                     "player::t_mutex: %ld\n",
                     guild_player->guild_id);
            fprintf (stderr, "RETURN NO TRACK SIZE\n");
        }

    return false;
}

void
Manager::handle_on_voice_ready (const dpp::voice_ready_t &event)
{
    const bool debug = get_debug_state ();
    this->clear_wait_vc_ready (event.voice_client->server_id);

    auto i = event.voice_client->get_tracks_remaining ();
    auto l = event.voice_client->get_secs_remaining ();

    if (debug)
        fprintf (stderr, "TO INSERT %d::%f\n", i, l);

    if (l < 0.05f)
        {
            event.voice_client->insert_marker ("r");
            if (debug)
                fprintf (stderr, "INSERTED \"r\" MARKER\n");
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
                && v->voiceclient->get_secs_remaining () > 0.05f
                && !v->voiceclient->is_paused ())
                {
                    if (event.from
                        && !this->is_manually_paused (event.state.guild_id))
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
                                                fprintf (stderr,
                                                         "Paused %ld as no "
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
                    // unpause if the track was paused automatically
                    if (v->channel_id == event.state.channel_id
                        && v->voiceclient->get_secs_remaining () > 0.05f
                        && v->voiceclient->is_paused ())
                        {
                            // Whether the track paused automatically
                            if (!this->is_manually_paused (
                                    event.state.guild_id))
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

                                            thread_manager::set_done ();
                                        },
                                        v->voiceclient);

                                    thread_manager::dispatch (tj);
                                }
                        }
                    // join user channel if no one listening in current channel
                    else if (event.state.channel_id
                             && v->channel_id != event.state.channel_id)
                        {
                            try
                                {
                                    if (debug)
                                        fprintf (
                                            stderr,
                                            "// join user channel if no one "
                                            "listening in current channel\n");

                                    this->full_reconnect (
                                        event.from, event.state.guild_id,
                                        v->channel_id, event.state.channel_id,
                                        true);
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

    // left vc
    if (!event.state.channel_id)
        {
            this->clear_disconnecting (event.state.guild_id);

            // update vcs cache
            vcs_setting_handle_disconnected (
                dpp::find_channel (event.state.channel_id));
        }
    // joined vc
    else
        {
            this->clear_connecting (event.state.guild_id);

            try
                {
                    auto a = get_voice_from_gid (event.state.guild_id,
                                                 event.state.user_id);

                    auto v = event.from->get_voice (event.state.guild_id);

                    if (v && v->voiceclient && v->voiceclient->is_ready ())
                        {
                            this->clear_wait_vc_ready (event.state.guild_id);
                        }

                    if (v && v->channel_id && v->channel_id != a.first->id)
                        {
                            this->stop_stream (event.state.guild_id);

                            if (v->voiceclient && !v->voiceclient->terminating)
                                {
                                    v->voiceclient->pause_audio (false);
                                    v->voiceclient->skip_to_next_marker ();
                                }

                            this->set_disconnecting (event.state.guild_id,
                                                     v->channel_id);

                            event.from->disconnect_voice (
                                event.state.guild_id);

                            if (has_listener (&a.second))
                                {
                                    this->set_connecting (event.state.guild_id,
                                                          a.first->id);

                                    this->set_waiting_vc_ready (
                                        event.state.guild_id);
                                }

                            std::thread tj ([this, event] () {
                                try
                                    {
                                        std::this_thread::sleep_for (
                                            std::chrono::seconds (1));

                                        this->voice_ready (
                                            event.state.guild_id, event.from);
                                    }
                                catch (...)
                                    {
                                    }

                                thread_manager::set_done ();
                            });

                            thread_manager::dispatch (tj);
                        }
                }
            catch (...)
                {
                }

            // update vcs cache
            vcs_setting_handle_connected (
                dpp::find_channel (event.state.channel_id));
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

void
Manager::prepare_play_stage_channel_routine (
    dpp::discord_voice_client *voice_client, dpp::guild *guild)
{
    if (!guild || !voice_client || !voice_client->creator)
        return;

    // stage channel
    dpp::channel *voice_channel = dpp::find_channel (voice_client->channel_id);

    if (!voice_channel || voice_channel->get_type () != dpp::CHANNEL_STAGE)
        return;

    const bool debug = get_debug_state ();

    auto i = guild->voice_members.find (this->sha_id);

    if (i == guild->voice_members.end ())
        return;
    // this will be true if you need to attempt to request speak
    const bool is_currently_suppressed = i->second.is_suppressed ();

    // return if not suppressed
    if (!is_currently_suppressed)
        return;

    // try not suppress if has
    // MUTE_MEMBERS permission
    const bool stay_suppress
        = has_permissions (guild, &voice_client->creator->me, voice_channel,
                           { dpp::p_mute_members })
              ? false
              : is_currently_suppressed;

    // set request_to_speak
    time_t request_ts = 0;
    if (stay_suppress
        && has_permissions (guild, &voice_client->creator->me, voice_channel,
                            { dpp::p_request_to_speak }))
        time (&request_ts);

    const bool update_voice_state
        = request_ts || (stay_suppress != is_currently_suppressed);

    if (update_voice_state)
        {
            if (debug)
                {
                    fprintf (stderr,
                             "[request_to_speak] "
                             "Requesting speak in "
                             "%ld\n",
                             i->second.channel_id);
                }

            voice_client->creator->current_user_set_voice_state (
                guild->id, i->second.channel_id, stay_suppress, request_ts);
        }
    else if (debug)
        {
            fprintf (stderr,
                     "[request_to_speak] "
                     "No "
                     "request_to_speak in "
                     "%ld\n",
                     i->second.channel_id);
        }
}

} // player
} // musicat
