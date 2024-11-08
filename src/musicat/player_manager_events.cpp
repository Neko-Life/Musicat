#include "musicat/cmds.h"
#include "musicat/db.h"
#include "musicat/events.h"
#include "musicat/mctrack.h"
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
            if (debug)
                fprintf (stderr,
                         "[Manager::handle_on_track_marker WARN] NO CLIENT\n");

            return false;
        }

    if (debug)
        std::cerr << "Handling voice marker: \"" << event.track_meta
                  << "\" in guild " << event.voice_client->server_id << '\n';

    prepare_play_stage_channel_routine (
        event.voice_client, dpp::find_guild (event.voice_client->server_id));

    auto guild_player = this->get_player (event.voice_client->server_id);

    if (!guild_player)
        {
            if (debug)
                fprintf (stderr,
                         "[Manager::handle_on_track_marker WARN] NO PLAYER\n");

            return false;
        }

    if (guild_player->processing_audio)
        {
            if (debug)
                std::cerr << "[Manager::handle_on_track_marker WARN] "
                             "PLAYER ALREADY PLAYING: "
                          << event.voice_client->server_id << "\n";

            return false;
        }

    this->clear_manually_paused (event.voice_client->server_id);

    if (this->is_disconnecting (event.voice_client->server_id))
        {
            if (debug)
                fprintf (stderr, "[Manager::handle_on_track_marker WARN] "
                                 "RETURN DISCONNECTING\n");

            return false;
        }

    if (debug)
        std::cerr
            << "[Manager::handle_on_track_marker] Locked player::t_mutex: "
            << guild_player->guild_id << '\n';

    std::lock_guard lk (guild_player->t_mutex);

    bool just_loaded_queue = false;
    if (guild_player->saved_queue_loaded != true)
        {
            this->load_guild_current_queue (event.voice_client->server_id,
                                            &sha_id);

            just_loaded_queue = true;
        }

    if (guild_player->saved_config_loaded != true)
        this->load_guild_player_config (event.voice_client->server_id);

    if (guild_player->queue.empty ())
        {
            if (debug)
                {
                    fprintf (stderr,
                             "[Manager::handle_on_track_marker WARN] NO SIZE "
                             "BEFORE: %d\n",
                             guild_player->loop_mode);
                }

            return false;
        }

    // Handle shifted tracks (tracks shifted to the front of the queue)
    guild_player->reset_shifted ();

    // Do stuff according to loop mode when playback ends
    // avoid this track to be skipped right away if it has different current
    // playback and first track entry
    if (event.track_meta == "e" && !guild_player->stopped
        && guild_player->current_track_is_first_track ())
        {
            int64_t rpt = guild_player->current_track.repeat;
            if (rpt > 0
                // in case of previous track erroring with repeat enabled
                // check if the to be current_track actually have repeat
                && guild_player->queue.front ().repeat > 0)
                {
                    guild_player->queue.front ().repeat
                        = --guild_player->current_track.repeat;
                }
            else
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
            const string removed_title
                = debug ? mctrack::get_title (guild_player->queue.front ())
                        : "";

            guild_player->queue.pop_front ();

            if (debug)
                {
                    std::cerr << "[Manager::handle_on_track_marker rm] Track "
                                 "removed "
                                 "in guild: `"
                              << removed_title << "` "
                              << guild_player->guild_id << '\n';
                }

            return false;
        }

    if (guild_player->queue.empty ())
        {
            if (debug)
                {
                    fprintf (stderr,
                             "[Manager::handle_on_track_marker WARN] NO SIZE "
                             "AFTER: %d\n",
                             guild_player->loop_mode);
                }

            if (!just_loaded_queue)
                database::delete_guild_current_queue (
                    event.voice_client->server_id);

            guild_player->current_track = MCTrack ();

            return false;
        }

    guild_player->queue.front ().skip_vote.clear ();
    // guild_player->queue.front ().seekable = false;

    guild_player->current_track = guild_player->queue.front ();
    guild_player->queue.front ().seek_to = "";

    guild_player->stopped = false;

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

    if (event.voice_client->get_secs_remaining () >= 0.05f)
        goto end_err;

    spawn_handle_track_marker_worker (event);

    return true;

end_err:
    std::cerr << "[Manager::handle_on_track_marker WARN] Voice "
                 "client not present or already playing: "
              << guild_player->guild_id << "\n";

    if (debug)
        {
            fprintf (stderr, "[Manager::handle_on_track_marker WARN] RETURN "
                             "NO TRACK SIZE\n");
        }

    return false;
}

void
Manager::handle_on_voice_ready (const dpp::voice_ready_t &event)
{
    const bool debug = get_debug_state ();

    auto guild_player = get_player (event.voice_client->server_id);
    if (guild_player)
        guild_player->voice_client = event.voice_client;

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
Manager::handle_non_sha_voice_state_update (
    const dpp::voice_state_update_t &event)
{
    bool debug = get_debug_state ();

    dpp::snowflake e_user_id = event.state.user_id;
    dpp::snowflake e_channel_id = event.state.channel_id;
    dpp::snowflake e_guild_id = event.state.guild_id;

    bool did_manually_paused = this->is_manually_paused (e_guild_id);

    dpp::voiceconn *v = event.from->get_voice (e_guild_id);

    if (!v || !v->channel_id || !v->voiceclient || v->voiceclient->terminating)
        return;

    bool is_playing_audio = v->voiceclient->get_secs_remaining () > 0.05f;

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

            this->full_reconnect (event.from, e_guild_id, v->channel_id,
                                  e_channel_id, true);

            return;
        }

    // Whether the track paused automatically
    // return if the track was paused manually
    if (!event.from || did_manually_paused)
        // not automatically paused
        return;

    // Pause audio when no user listening in vc: autopause
    if (should_pause)
        {
            if (this->set_autopause (v, event.state.guild_id))
                return;

            if (!debug)
                return;

            std::cerr << "Paused " << event.state.guild_id
                      << " as no user in vc\n";

            return;
        }

    // unpause
    // dispatch autoresume job
    int tstatus
        = timer::create_resume_timer (e_user_id, e_channel_id, v->voiceclient);

    if (tstatus != 0)
        {
            std::cerr << "[Manager::handle_on_voice_state_update WARN] "
                         "timer::create_resume_timer uid("
                      << e_user_id << ") sid(" << e_guild_id << ") svcid("
                      << e_channel_id << ") status(" << tstatus << ")\n";
        }
}

void
Manager::handle_sha_voice_state_update (const dpp::voice_state_update_t &event)
{
    bool debug = get_debug_state ();

    dpp::snowflake e_user_id = event.state.user_id;
    dpp::snowflake e_channel_id = event.state.channel_id;
    dpp::snowflake e_guild_id = event.state.guild_id;

    bool did_manually_paused = this->is_manually_paused (e_guild_id);

    // left vc
    if (!e_channel_id)
        {
            this->clear_disconnecting (e_guild_id);

            // remove voice_client pointer from guild player
            if (auto guild_player = get_player (e_guild_id); guild_player)
                guild_player->voice_client = nullptr;

            // update vcs cache
            vcs_setting_handle_disconnected (dpp::find_channel (e_channel_id));
            return;
        }

    // joined vc
    this->clear_connecting (e_guild_id);

    auto v = event.from->get_voice (e_guild_id);

    // declare vars here for clean code standard!!!!
    std::pair<dpp::channel *, std::map<dpp::snowflake, dpp::voicestate> > a
        = { nullptr, {} };
    std::thread tj;
    std::pair<dpp::channel *, dpp::voicestate *> cached = { nullptr, nullptr };
    bool new_state_muted = false, old_state_muted = false, is_paused = false;
    int tstatus = 1;

    if (!v)
        // no conn, skip everything
        goto end;

    a = get_voice_from_gid (e_guild_id, e_user_id);

    if (v->voiceclient && v->voiceclient->is_ready ())
        {
            // reset waiting vc ready state
            this->clear_wait_vc_ready (e_guild_id);
        }

    if (!a.first || !v->channel_id)
        goto end;

    // reconnect when bot user is in vc but no vc state in
    // dpp cache can means bot stays in vc after reboot
    // situation
    if (v->channel_id == a.first->id)
        goto skip_reconnect;

#ifdef USE_VOICE_SERVER_UPDATE_RECONNECT
    // Update channel id manually when moved to different voice channel
    // !TODO: this is the time to set previous playback position we can reseek
    v->channel_id = a.first->id;

    goto skip_reconnect;
#endif

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

    tj = std::thread (
        [this, e_guild_id] (dpp::discord_client *from) {
            thread_manager::DoneSetter tmds;

            std::this_thread::sleep_for (std::chrono::seconds (1));

            this->voice_ready (e_guild_id, from);
        },
        event.from);

    thread_manager::dispatch (tj);
    // reconnecting, shouldn't check for autopause, skip it
    goto end;
skip_reconnect:
    // else block, autopause check

    // not modifying connection, check for server mute if
    // not manually paused
    if (!v->voiceclient || did_manually_paused)
        goto end;
    //
    // get state cache
    cached = vcs_setting_get_cache (v->channel_id);

    new_state_muted = event.state.is_mute ();
    old_state_muted = cached.second && cached.second->is_mute ();

    // * new state has channel
    //
    // autopause if muted and vice versa
    if (!cached.second)
        // skip if state has no voice channel
        goto end;

    is_paused = v->voiceclient->is_paused ();

    if (is_paused || !new_state_muted || old_state_muted)
        goto skip_autopause;
    // server muted, set autopause
    if (this->set_autopause (v, e_guild_id, false) || !debug)
        // skip if no debug
        goto end;

    std::cerr << "Paused " << event.state.guild_id << " as server muted\n";
    goto end;

skip_autopause:
    // else block
    if (!is_paused || new_state_muted || !old_state_muted)
        // no condition met to resume, skip resuming
        goto end;

    // server unmuted, resume
    // dispatch autoresume job
    tstatus = timer::create_resume_timer (e_user_id, e_channel_id,
                                          v->voiceclient, 0);

    if (tstatus == 0)
        // no error, skip warn
        goto end;

    std::cerr << "[Manager::handle_on_voice_state_update WARN] "
                 "timer::create_resume_timer uid("
              << e_user_id << ") sid(" << e_guild_id << ") svcid("
              << e_channel_id << ") status(" << tstatus << ")\n";

end:
    // update vcs cache
    vcs_setting_handle_connected (dpp::find_channel (e_channel_id),
                                  &event.state);
    // if (muted) player_manager->pause(event.guild_id);
    // else player_manager->resume(guild_id);
}

void
Manager::handle_on_voice_state_update (const dpp::voice_state_update_t &event)
{
    dpp::snowflake sha_id = get_sha_id ();
    dpp::snowflake e_user_id = event.state.user_id;

    bool is_not_sha_event = e_user_id != sha_id;

    // Non sha event
    if (!is_not_sha_event)
        goto skip_not_sha_event;

    handle_non_sha_voice_state_update (event);
    // End non sha event
    return;
skip_not_sha_event:

    // Client user code -----------------------------------
    handle_sha_voice_state_update (event);
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

void
Manager::spawn_handle_track_marker_worker (
    const dpp::voice_track_marker_t &event)
{
    std::thread tj = std::thread (
        [this] (dpp::discord_voice_client *v, const string meta) {
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

            const bool debug = get_debug_state ();

            std::lock_guard lk (guild_player->t_mutex);
            guild_player->voice_client = v;
            MCTrack &track = guild_player->current_track;

            // text channel to send embed
            dpp::snowflake channel_id = guild_player->channel_id;

            this->wait_for_vc_ready (guild_id);

            // channel for sending message
            dpp::channel *c = dpp::find_channel (channel_id);
            // guild
            dpp::guild *g = dpp::find_guild (guild_id);

            this->wait_for_download (track.filename);

            const string track_id = mctrack::get_id (track);

            if (guild_player->max_history_size > 0)
                {
                    guild_player->history.push_back (track_id);

                    const size_t cur_hs = guild_player->history.size ();
                    if (cur_hs > guild_player->max_history_size)
                        {
                            guild_player->history.erase (
                                guild_player->history.begin (),
                                guild_player->history.begin ()
                                    + (cur_hs
                                       - guild_player->max_history_size));
                        }
                }

            const bool embed_perms
                = has_permissions (g, &this->cluster->me, c,
                                   { dpp::p_view_channel, dpp::p_send_messages,
                                     dpp::p_embed_links });

            {
                const string fname = track.filename;

                const string absolute_path = get_music_folder_path () + fname;

                std::ifstream test (absolute_path,
                                    std::ios_base::in | std::ios_base::binary);

                // redownload vars
                string track_info_m;
                bool is_downloading = false;

                if (test.is_open ())
                    {
                        test.close ();
                        goto has_file;
                    }

                track_info_m
                    = embed_perms
                          ? '`' + mctrack::get_title (track) + "` (added by <@"
                                + std::to_string (track.user_id) + ">)"
                          : "";
                is_downloading = this->waiting_file_download.find (fname)
                                 != this->waiting_file_download.end ();

                const int failed_playback
                    = get_track_failed_playback_count (track.filename);
                const bool dont_retry = failed_playback >= 3;

                if (dont_retry)
                    {
                        // create timer to allow the track to be played again
                        // later
                        timer::create_failed_playback_reset_timer (fname);
                    }

                if (embed_perms)
                    {
                        string m_content_redo;

                        if (dont_retry)
                            {
                                m_content_redo
                                    = "Track " + track_info_m
                                      + " failed to play for too many times, "
                                        "I shall stop trying to play it for a "
                                        "while. Sorry";
                            }
                        else
                            m_content_redo
                                = (is_downloading ? "Missing audio file, "
                                                    "awaiting download: "
                                                  : "Missing audio file: ")
                                  + track_info_m
                                  + (is_downloading
                                         ? ""
                                         : "\nTrying to redownload...\n"
                                           "I will add this track to "
                                           "the top of the queue once "
                                           "it's done downloading");

                        dpp::message m (channel_id, m_content_redo);

                        this->cluster->message_create (m);
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

                        // skip current track
                        guild_player->skip_queue (1, true, true);

                        // stop playing next song if failed trying 3 times
                        if (!dont_retry)
                            {
                                set_track_failed_playback_count (
                                    track.filename, failed_playback + 1);

                                // set stopped to avoid marker handler
                                // popping the next song as its already skipped
                                // above
                                guild_player->stopped = true;

                                // play next song
                                v->insert_marker ("e");
                            }
                    }

                if (!is_downloading && !dont_retry)
                    {
                        // try redownload
                        this->waiting_file_download[fname] = guild_id;
                        this->download (fname, mctrack::get_url (track),
                                        guild_id);

                        std::thread dlt (
                            [this, guild_player, guild_id, fname,
                             v] (player::MCTrack result) {
                                thread_manager::DoneSetter tmds;

                                this->wait_for_download (fname);

                                guild_player->add_track (result, true,
                                                         guild_id, true);

                                // decide whether to trigger track marker after
                                // dowload
                                if (v && !v->terminating && guild_player->from)
                                    {
                                        player::decide_play (
                                            guild_player->from, guild_id,
                                            false);
                                    }
                            },
                            track);

                        thread_manager::dispatch (dlt);
                    }

                // no audio file
                return;
            }

        has_file:
            set_track_failed_playback_count (track.filename, 0);
            // make sure it has no reset timer active in case it failed again
            timer::remove_failed_playback_reset_timer (track.filename);

            if (meta == "r")
                v->send_silence (60);

            // check for autoplay
            if (guild_player->auto_play)
                this->get_next_autoplay_track (track_id, guild_player->from,
                                               guild_id);

            try
                {
                    int pstatus = this->play (guild_id, track, channel_id);

                    if (!guild_player->notification)
                        return;

                    // Send play info embed
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
}

} // player
} // musicat
