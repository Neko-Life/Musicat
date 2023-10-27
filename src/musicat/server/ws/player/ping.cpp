#include "musicat/musicat.h"
#include "musicat/server/ws/player.h"
#include <uWebSockets/src/App.h>

namespace musicat::server::ws::player::events
{

void
ping (uws_ws_t *ws, std::string_view msg)
{
    const bool debug = get_debug_state ();

    if (debug)
        {
            fprintf (stderr, "[server PING] %lu: %s\n", (uintptr_t)ws,
                     std::string (msg).c_str ());
        }
}

} // musicat::server::ws::player::events
