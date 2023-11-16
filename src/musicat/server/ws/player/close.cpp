#include "musicat/musicat.h"
#include "musicat/server/ws/player.h"
#include <uWebSockets/src/App.h>

namespace musicat::server::ws::player::events
{

void
close (uws_ws_t *ws, int code, std::string_view message)
{
    const bool debug = get_debug_state ();
    // You may access ws->getUserData() here

    if (debug)
        {
            fprintf (stderr, "[server CLOSE] %lu %d: %s\n", (uintptr_t)ws,
                     code, std::string (message).c_str ());
        }
}

} // musicat::server::ws::player::events
