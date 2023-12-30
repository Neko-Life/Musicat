#include "musicat/cmds.h"
#include "musicat/db.h"
#include "musicat/musicat.h"
#include "musicat/player.h"
#include "musicat/player_manager_timer.h"
#include "musicat/thread_manager.h"
#include <memory>

namespace musicat
{
namespace player
{
// this section can't be anymore horrible than this....
using string = std::string;

// !!TODO: remove shared_manager and use get_player_manager_ptr() instead!!!
// do that for everything requires player_manager!!!!!!!!
//
// btw shared_manager can be simply `this` smfh
bool
Manager::handle_on_track_marker (const dpp::voice_track_marker_t &event)
{
    const bool debug = get_debug_state ();
    const auto sha_id = get_sha_id ();

    if (!event.voice_client || event.voice_client->terminating
        || !get_running_state ())
        {
            fprintf (stderr, "NO CLIENT\n");
            return false;
        }

    if (debug)
        std::cerr << "Handling voice marker: \"" << event.track_meta
                  << "\" in guild " << event.voice_client->server_id << '\n';

    this->clear_manually_paused (event.voice_client->server_id);

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
        std::cerr
            << "[Manager::handle_on_track_marker] Locked player::t_mutex: "
            << guild_player->guild_id << '\n';

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
                }

            return false;
        }

    // Handle shifted tracks (tracks shifted to the front of the queue)
    guild_player->reset_shifted ();

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
            const string removed_title = guild_player->queue.front ().title ();
            guild_player->queue.pop_front ();

            if (debug)
                std::cerr
                    << "[Manager::handle_on_track_marker rm] Track removed "
                       "in guild: `"
                    << removed_title << "` " << guild_player->guild_id << '\n';

            return false;
        }

    if (guild_player->queue.size () == 0)
        {
            if (debug)
                {
                    fprintf (stderr, "NO SIZE AFTER: %d\n",
                             guild_player->loop_mode);
                }

            if (!just_loaded_queue)
                database::delete_guild_current_queue (
                    event.voice_client->server_id);

            return false;
        }

    guild_player->queue.front ().skip_vote.clear ();
    guild_player->queue.front ().seekable = false;

    guild_player->current_track = guild_player->queue.front ();
    guild_player->queue.front ().seek_to = "";

    guild_player->set_stopped (false);

    if (!just_loaded_queue)
        database::update_guild_current_queue (event.voice_client->server_id,
                                              guild_player->queue);

    auto c = get_voice_from_gid (event.voice_client->server_id, sha_id);

    if (!c.first || !has_listener (&c.second))
        {
            if (debug)
                std::cerr << "[Manager::handle_on_track_marker] No "
                             "listener in voice channel: "
                          << event.voice_client->channel_id << " ("
                          << guild_player->guild_id << ")\n";

            return false;
        }

    std::thread tj;

    if (event.voice_client->get_secs_remaining () >= 0.05f)
        goto end_err;

    tj = std::thread (
        [this] (dpp::discord_voice_client *v, string meta) {
            thread_manager::DoneSetter tmds;

            if (!v || v->terminating)
                {
                    std::cerr << "[Manager::handle_on_track_marker::tj ERROR] "
                                 "Voice client is null: "
                              << meta << '\n';

                    return;
                }

            auto guild_id = v->server_id;
            auto guild_player = this->get_player (guild_id);

            if (!guild_player)
                {
                    std::cerr << "[Manager::handle_on_track_marker::tj ERROR] "
                                 "No player in guild: "
                              << guild_id << '\n';

                    return;
                }

            MCTrack &track = guild_player->current_track;

            std::lock_guard<std::mutex> lk (guild_player->t_mutex);

            // text channel to send embed
            dpp::snowflake channel_id = guild_player->channel_id;

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

            // check for autoplay
            const string track_id = track.id ();
            std::thread at_t;
            if (!guild_player->auto_play)
                goto no_autoplay;

            at_t = std::thread (
                [this] (const std::string &track_id, dpp::discord_client *from,
                        const dpp::snowflake &server_id) {
                    thread_manager::DoneSetter tmds;

                    this->get_next_autoplay_track (track_id, from, server_id);
                },
                track_id, guild_player->from, guild_player->guild_id);

            thread_manager::dispatch (at_t);

        no_autoplay:
            if (guild_player->max_history_size)
                {
                    guild_player->history.push_back (track_id);

                    while (guild_player->history.size ()
                           > guild_player->max_history_size)
                        {
                            guild_player->history.pop_front ();
                        }
                }

            bool embed_perms
                = has_permissions (g, &this->cluster->me, c,
                                   { dpp::p_view_channel, dpp::p_send_messages,
                                     dpp::p_embed_links });

            {
                const string absolute_path
                    = get_music_folder_path () + track.filename;

                std::ifstream test (absolute_path,
                                    std::ios_base::in | std::ios_base::binary);

                if (test.is_open ())
                    {
                        test.close ();
                        goto has_file;
                    }

                fprintf (stderr,
                         "[Manager::handle_on_track_marker tj ERROR] "
                         "Can't open audio file: %s\n",
                         absolute_path.c_str ());

                // file not found, might be download error or
                // deleted
                if (v && !v->terminating)
                    {
                        fprintf (stderr,
                                 "[Manager::handle_on_track_marker tj ERROR] "
                                 "Inserting `e` marker\n");

                        this->remove_ignore_marker (guild_id);

                        // let the user decide what to do instead of straight
                        // skipping the unplayable track, maybe i want to save
                        // it to playlist first!
                        //
                        // v->insert_marker ("e");
                    }

                // can't notify user, what else to do?
                if (!embed_perms)
                    {
                        return;
                    }

                const string m_content
                    = "Can't play track: " + track.title () + " (added by <@"
                      + std::to_string (track.user_id) + ">)";

                dpp::message m (channel_id, m_content);

                this->cluster->message_create (m);

                // no audio file
                return;
            }

        has_file:
            if (meta == "r")
                v->send_silence (60);

            // Send play info embed
            try
                {
                    int pstatus = this->play (v, track, channel_id);

                    bool should_update_embed = false,
                         not_repeating_song = false;

                    if (!embed_perms)
                        goto log_no_embed;

                    // Update if last message is the
                    // info embed message
                    should_update_embed
                        = c && guild_player->info_message && c->last_message_id
                          && c->last_message_id
                                 == guild_player->info_message->id;

                    if (!should_update_embed)
                        goto del_info_embed;

                    not_repeating_song
                        = (guild_player->loop_mode != loop_mode_t::l_song)
                          && (guild_player->loop_mode
                              != loop_mode_t::l_song_queue);

                    if (not_repeating_song)
                        this->update_info_embed (guild_id, true);

                    return;

                del_info_embed:
                    this->delete_info_embed (guild_id);

                    this->send_info_embed (guild_id, false, true);
                    return;

                log_no_embed:
                    fprintf (stderr, "[EMBED_UPDATE] No channel or "
                                     "permission to send info embed\n");
                    return;
                }
            catch (const exception &e)
                {
                    fprintf (stderr, "[ERROR EMBED_UPDATE] %s\n", e.what ());

                    auto cd = e.code ();

                    if (!embed_perms || (cd != 1 && cd != 2))
                        return;

                    dpp::message m;
                    m.set_channel_id (channel_id).set_content (e.what ());

                    this->cluster->message_create (m);
                }
        },
        event.voice_client, event.track_meta);

    thread_manager::dispatch (tj);

    return true;

end_err:
    std::cerr << "[Manager::handle_on_track_marker WARN] Voice "
                 "client not present or already playing: "
              << guild_player->guild_id << "\n";

    if (debug)
        {
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
    bool debug = get_debug_state ();
    dpp::snowflake sha_id = get_sha_id ();
    dpp::snowflake e_user_id = event.state.user_id;
    bool is_not_sha_event = e_user_id != sha_id;
    dpp::snowflake e_channel_id = event.state.channel_id;
    dpp::snowflake e_guild_id = event.state.guild_id;

    // Non sha event
    if (is_not_sha_event)
        {
            dpp::voiceconn *v = event.from->get_voice (e_guild_id);

            if (!v || !v->channel_id || !v->voiceclient
                || v->voiceclient->terminating)
                return;

            bool is_playing_audio
                = v->voiceclient->get_secs_remaining () > 0.05f;

            if (!is_playing_audio)
                return;

            bool has_voice_session = v->channel_id == event.state.channel_id;
            bool is_paused = v->voiceclient->is_paused ();

            bool should_pause = !has_voice_session && !is_paused;
            bool should_unpause = has_voice_session && is_paused;

            // join user channel if no one listening in current channel
            if (!should_pause && !should_unpause)
                {
                    if (!e_channel_id || has_voice_session)
                        return;

                    if (debug)
                        fprintf (stderr, "// join user channel if no one "
                                         "listening in current channel\n");

                    this->full_reconnect (event.from, e_guild_id,
                                          v->channel_id, e_channel_id, true);

                    return;
                }

            bool is_manually_paused = this->is_manually_paused (e_guild_id);

            // Pause audio when no user listening in vc
            if (should_pause)
                {
                    if (!event.from || is_manually_paused)
                        return;

                    auto voice
                        = get_voice_from_gid (event.state.guild_id, sha_id);

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
                            return;
                        }

                exec_pause_audio:
                    v->voiceclient->pause_audio (true);
                    this->update_info_embed (event.state.guild_id);

                    if (!debug)
                        return;

                    std::cerr << "Paused " << event.state.guild_id
                              << " as no user in vc\n";

                    return;
                }

            // unpause if the track was paused automatically

            // Whether the track paused automatically
            if (is_manually_paused)
                // not automatically paused
                return;

            // dispatch auto resume job
            int tstatus = timer::create_resume_timer (e_user_id, e_channel_id,
                                                      v->voiceclient);

            if (tstatus != 0)
                {
                    std::cerr
                        << "[Manager::handle_on_voice_state_update WARN] "
                           "timer::create_resume_timer uid("
                        << e_user_id << ") sid(" << e_guild_id << ") svcid("
                        << e_channel_id << ") status(" << tstatus << ")\n";
                }

            // End non sha event
            return;
        }

    // Client user code -----------------------------------

    // left vc
    if (!e_channel_id)
        {
            this->clear_disconnecting (e_guild_id);

            // update vcs cache
            vcs_setting_handle_disconnected (dpp::find_channel (e_channel_id));
            return;
        }

    // joined vc
    this->clear_connecting (e_guild_id);

    auto a = get_voice_from_gid (e_guild_id, e_user_id);
    auto v = event.from->get_voice (e_guild_id);

    if (v && v->voiceclient && v->voiceclient->is_ready ())
        {
            this->clear_wait_vc_ready (e_guild_id);
        }

    // reconnect when bot user is in vc but no vc state in cache
    // can means bot stays in vc after reboot situation
    if (v && v->channel_id && v->channel_id != a.first->id)
        {
            this->stop_stream (e_guild_id);

            if (v->voiceclient && !v->voiceclient->terminating)
                {
                    v->voiceclient->pause_audio (false);
                    v->voiceclient->skip_to_next_marker ();
                }

            this->set_disconnecting (e_guild_id, v->channel_id);

            event.from->disconnect_voice (e_guild_id);

            if (has_listener (&a.second))
                {
                    this->set_connecting (e_guild_id, a.first->id);

                    this->set_waiting_vc_ready (e_guild_id);
                }

            std::thread tj (
                [this, e_guild_id] (dpp::discord_client *from) {
                    thread_manager::DoneSetter tmds;

                    std::this_thread::sleep_for (std::chrono::seconds (1));

                    this->voice_ready (e_guild_id, from);
                },
                event.from);

            thread_manager::dispatch (tj);
        }

    // update vcs cache
    vcs_setting_handle_connected (dpp::find_channel (e_channel_id));
    // if (muted) player_manager->pause(event.guild_id);
    // else player_manager->resume(guild_id);
}

void
Manager::handle_on_message_delete (const dpp::message_delete_t &event)
{
    this->set_info_message_as_deleted (event.id);
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

    auto i = guild->voice_members.find (get_sha_id ());

    // sha not in guild cache?? that's weird
    if (i == guild->voice_members.end ())
        return;

    // this will be true if you need to attempt to request speak
    const bool is_currently_suppressed = i->second.is_suppressed ();

    // return if not suppressed
    if (!is_currently_suppressed)
        return;

    const bool debug = get_debug_state ();

    // try not suppress if has
    // MUTE_MEMBERS permission
    const bool stay_suppress
        = has_permissions (guild, &voice_client->creator->me, voice_channel,
                           { dpp::p_mute_members })
              ? false
              : is_currently_suppressed;

    // set request_to_speak, if has request to speak permission
    time_t request_ts = 0;
    if (stay_suppress
        && has_permissions (guild, &voice_client->creator->me, voice_channel,
                            { dpp::p_request_to_speak }))
        time (&request_ts);

    const bool update_voice_state
        = request_ts || (stay_suppress != is_currently_suppressed);

    if (!update_voice_state)
        {
            if (debug)
                {
                    std::cerr
                        << "[Manager::prepare_play_stage_channel_routine] "
                           "No request_to_speak in "
                        << i->second.channel_id << '\n';
                }

            return;
        }

    if (debug)
        {
            std::cerr << "[Manager::prepare_play_stage_channel_routine] "
                         "Requesting speak in "
                      << i->second.channel_id << '\n';
        }

    voice_client->creator->current_user_set_voice_state (
        guild->id, i->second.channel_id, stay_suppress, request_ts);
}

} // player
} // musicat
