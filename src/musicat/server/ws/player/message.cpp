#include "musicat/musicat.h"
#include "musicat/server/ws/player.h"
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

inline constexpr const socket_event_handler_t socket_event_handlers[] = {

    { SOCKET_EVENT_PAUSE, _stub },
    { SOCKET_EVENT_PLAY, _stub },
    { SOCKET_EVENT_SEEK, _stub },
    { SOCKET_EVENT_STOP, _stub },
    { SOCKET_EVENT_FX, _stub },
    { SOCKET_EVENT_QUEUE, _stub },
    { SOCKET_EVENT_REGISTER, handle_register },
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
