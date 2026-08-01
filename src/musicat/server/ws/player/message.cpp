#include "musicat/cmds/seek.h"
#include "musicat/musicat.h"
#include "musicat/server/ws/player.h"
#include <cstdint>
#include <uWebSockets/src/App.h>

namespace musicat::server::ws::player::events
{

static int
_stub (const nlohmann::json &data, uws_ws_t *ws)
{
    fprintf (stderr, "[server::ws::player::events::_stub] %ld:\n%s\n\n", ws, data.dump ().c_str ());
    return 0;
}

static int
handle_register (const nlohmann::json &data, uws_ws_t *ws)
{
    // let it be object in case we wanna add other stuff later on
    if (!data.is_object ())
        return 1;
    auto i_uid = data.find ("uid");
    if (i_uid == data.end () || !i_uid->is_string ())
        return 1;

    register_ws_user (ws, i_uid->get<std::string> ());

    return 0;
}

static int
handle_pause (const nlohmann::json &data, uws_ws_t *ws)
{
    auto *player_manager = get_player_manager_ptr ();
    if (!player_manager)
        return -1;

    auto *sdata = ws->getUserData ();
    auto guild_player = player_manager->get_player (sdata->server_id);
    if (!guild_player)
        return -1;

    auto *from = guild_player->get_client ();
    // this should never happen
    if (!from)
        {
            nlohmann::json d = nlohmann::json::object ({ { "e", SOCKET_EVENT_ERROR }, { "d", "Guild have no managing client" } });
            ws->send (d.dump ());
        }

    if (from)
        {
            // !TODO: check if user actually in the same vc session
            player_manager->pause (from, sdata->server_id, sdata->user_id);
            publish_pause (sdata->server_id);
        }
    else
        {
            nlohmann::json d2 = nlohmann::json::object ({ { "e", SOCKET_EVENT_PAUSE }, { "d", nullptr } });
            ws->send (d2.dump ());
        }

    return 0;
}

static int
handle_play (const nlohmann::json &data, uws_ws_t *ws)
{
    auto *player_manager = get_player_manager_ptr ();
    if (!player_manager)
        return -1;

    auto *sdata = ws->getUserData ();
    auto guild_player = player_manager->get_player (sdata->server_id);
    if (!guild_player)
        return -1;

    bool continued = false;
    auto *voiceclient = guild_player->get_voice_client ();
    if (voiceclient)
        {
            if (voiceclient->is_paused ())
                {
                    // !TODO: check if user actually in the same vc session
                    player_manager->unpause (voiceclient, sdata->server_id, true);
                    continued = true;
                }
            else if (!guild_player->processing_audio && guild_player->queue.size ())
                {
                    voiceclient->stop_audio ();
                    voiceclient->insert_marker ("c");
                    publish_play (sdata->server_id);
                    continued = true;
                }
        }
    else
        player_manager->check_health (sdata->server_id);

    if (!continued)
        {
            nlohmann::json d = nlohmann::json::object ({ { "e", SOCKET_EVENT_PLAY }, { "d", nullptr } });
            ws->send (d.dump ());
        }
    else
        {
            try
                {
                    player_manager->update_info_embed (sdata->server_id, false);
                }
            catch (...)
                {
                    // AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
                }
        }

    return 0;
}

static int
handle_seek (const nlohmann::json &data, uws_ws_t *ws)
{
    auto *player_manager = get_player_manager_ptr ();
    if (!player_manager)
        return -1;

    auto *sdata = ws->getUserData ();
    auto guild_player = player_manager->get_player (sdata->server_id);
    if (!guild_player)
        return -1;

    if (!guild_player->processing_audio)
        {
            const nlohmann::json d = nlohmann::json::object ({ { "e", SOCKET_EVENT_SEEK }, { "d", data } });
            ws->send (d.dump ());
            return 0;
        }

    if (!data.is_number_unsigned ())
        {
            // invalid data
            return -1;
        }

    uint64_t total_ms = data.get<uint64_t> ();

    // !TODO: check if user actually in the same vc session
    guild_player->current_track.current_byte = (int64_t)(::musicat::player::opus_byte_per_ms * total_ms);
    guild_player->current_track.seek_to = "y";
    publish_seek (sdata->server_id, total_ms);

    try
        {
            player_manager->update_info_embed (sdata->server_id, false);
        }
    catch (...)
        {
            // AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
        }

    return 0;
}

static int
handle_stop (const nlohmann::json &data, uws_ws_t *ws)
{
    auto *player_manager = get_player_manager_ptr ();
    if (!player_manager)
        return -1;

    auto *sdata = ws->getUserData ();
    auto guild_player = player_manager->get_player (sdata->server_id);
    if (!guild_player)
        return -1;

    auto *voiceclient = guild_player->get_voice_client ();
    if (!voiceclient)
        {
            const nlohmann::json d = nlohmann::json::object ({ { "e", SOCKET_EVENT_STOP }, { "d", nullptr } });
            ws->send (d.dump ());
            return 0;
        }

    // !TODO: check if user actually in the same vc session
    guild_player->skip_playback (voiceclient);
    guild_player->stopped = true;
    voiceclient->pause_audio (true);

    player_manager->set_manually_paused (sdata->server_id);

    publish_stop (sdata->server_id);

    try
        {
            player_manager->update_info_embed (sdata->server_id, false);
        }
    catch (...)
        {
            // AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
        }

    return 0;
}

static int
handle_next (const nlohmann::json &data, uws_ws_t *ws)
{
    auto *player_manager = get_player_manager_ptr ();
    if (!player_manager)
        return -1;

    auto *sdata = ws->getUserData ();
    auto guild_player = player_manager->get_player (sdata->server_id);
    if (!guild_player)
        return -1;

    auto *voiceclient = guild_player->get_voice_client ();

    guild_player->skip_queue (1);
    server::ws::player::publish_queue (sdata->server_id);

    guild_player->skip_playback (voiceclient);

    try
        {
            player_manager->update_info_embed (sdata->server_id, false);
        }
    catch (...)
        {
            // AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
        }

    return 0;
}

inline constexpr const int64_t second_seek_step = SECOND_SEEK_STEP * 1000;

static int
handle_prev (const nlohmann::json &data, uws_ws_t *ws)
{
    auto *player_manager = get_player_manager_ptr ();
    if (!player_manager)
        return -1;

    auto *sdata = ws->getUserData ();
    auto guild_player = player_manager->get_player (sdata->server_id);
    if (!guild_player)
        return -1;
    auto *voiceclient = guild_player->get_voice_client ();
    if (!voiceclient)
        return 0;

    ::musicat::player::track_progress prog = util::get_track_progress (guild_player->current_track);
    if (prog.current_ms >= second_seek_step)
        {
            nlohmann::json d = nlohmann::json::number_unsigned_t (0);
            return handle_seek (d, ws);
        }

    std::lock_guard lk (guild_player->t_mutex);
    guild_player->reset_shifted ();

    if (!guild_player->current_track.is_empty ())
        guild_player->current_track.repeat = 0;

    if (!guild_player->queue.empty ())
        {
            guild_player->queue.front ().repeat = 0;

            // move the back of the queue to front
            guild_player->queue_add_front (guild_player->queue.back ());
            guild_player->queue_pop ();

            server::ws::player::publish_queue (sdata->server_id);

            guild_player->skip_playback (voiceclient);
        }
    else
        {
            fprintf (stderr, "[server::ws::player::events::handle_prev WARN] Track queue is empty: %s\n", sdata->server_id.str ().c_str ());

            const nlohmann::json d = nlohmann::json::object ({ { "e", SOCKET_EVENT_PLAY }, { "d", nullptr } });
            ws->send (d.dump ());
        }

    try
        {
            player_manager->update_info_embed (sdata->server_id, false);
        }
    catch (...)
        {
            // AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
        }

    return 0;
}

inline constexpr const socket_event_handler_t socket_event_handlers[] = {

    { SOCKET_EVENT_PAUSE, handle_pause },
    { SOCKET_EVENT_PLAY, handle_play },
    { SOCKET_EVENT_SEEK, handle_seek },
    { SOCKET_EVENT_STOP, handle_stop },
    // { SOCKET_EVENT_FX, _stub },
    // { SOCKET_EVENT_QUEUE, _stub },
    { SOCKET_EVENT_REGISTER, handle_register },
    { SOCKET_EVENT_NEXT, handle_next },
    { SOCKET_EVENT_PREV, handle_prev },
    { SOCKET_EVENT_ERROR, NULL }
};

int
handle_message (const socket_event_e e, const nlohmann::json &payload, uws_ws_t *ws)
{
    int (*handler) (const nlohmann::json &, uws_ws_t *) = NULL;

    for (size_t i = 0; i < (sizeof (socket_event_handlers) / sizeof (*socket_event_handlers)); i++)
        {
            const socket_event_handler_t *seh = &socket_event_handlers[i];

            if (seh->event == SOCKET_EVENT_ERROR && !seh->handler)
                break;

            if (seh->event != e)
                continue;

            handler = seh->handler;
            break;
        }

    if (!handler)
        return 0;

    nlohmann::json data;
    auto i_d = payload.find ("d");
    if (i_d != payload.end ())
        data = *i_d;

    return handler (data, ws);
}

void
message (uws_ws_t *ws, std::string_view msg, uWS::OpCode code)
{
    const bool debug = get_debug_state ();

    if (debug)
        {
            fprintf (stderr, "[server MESSAGE] %lu %d: %s\n", (uintptr_t)ws, code, std::string (msg).c_str ());
        }

    if (msg.empty ())
        return;

    if (msg == "meow!")
        {
            ws->getUserData ()->waved = true;
            ws->send ("(^v^)");
            return;
        }

    if (msg == "0")
        {
            ws->send ("0");
            return;
        }

    try
        {
            nlohmann::json json_payload = nlohmann::json::parse (msg);

            bool is_object = json_payload.is_object ();
            auto i_e = json_payload.find ("e");

            if (!is_object || i_e == json_payload.end () || !i_e->is_number ())
                {
                    ws->close ();
                    return;
                }

            int status = handle_message ((socket_event_e)i_e->get<int64_t> (), json_payload, ws);
            if (status)
                {
                    ws->close ();

                    fprintf (stderr, "[server::ws::player::events::message ERROR] payload:\n%s\n\nstatus: %d\n",
                             json_payload.dump ().c_str (), status);
                }
        }
    catch (const nlohmann::json::exception &e)
        {
            ws->close ();

            fprintf (stderr, "[server::ws::player::events::message ERROR] %s\n", e.what ());

            return;
        }
}

} // musicat::server::ws::player::events
