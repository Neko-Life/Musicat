#include "musicat/musicat.h"
#include "musicat/server/ws/player.h"
#ifndef MUSICAT_NO_SERVER
#include <uWebSockets/src/App.h>
#endif

namespace musicat::server::ws::player::events
{
#ifndef MUSICAT_NO_SERVER

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

#endif
} // musicat::server::ws::player::events
