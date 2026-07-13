#include "musicat/musicat.h"
#include "musicat/server/ws/player.h"
#include <uWebSockets/src/App.h>

namespace musicat::server::ws::player::events
{

inline constexpr const socket_event_handler_t socket_event_handlers[] = { { SOCKET_EVENT_ERROR, NULL } };

void
handle_message (const socket_event_e e, const nlohmann::json &payload)
{
    void (*handler) (const nlohmann::json &data) = NULL;

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
        return;

    nlohmann::json data;
    auto i_d = payload.find ("d");
    if (i_d != payload.end ())
        data = *i_d;

    handler (data);
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

            handle_message ((socket_event_e)i_e->get<int64_t> (), json_payload);
        }
    catch (const nlohmann::json::exception &e)
        {
            ws->close ();

            fprintf (stderr, "[server::ws::player::events::message ERROR] %s\n", e.what ());

            return;
        }
}

} // musicat::server::ws::player::events
