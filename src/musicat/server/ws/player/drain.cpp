#include "musicat/musicat.h"
#include "musicat/server/ws/player.h"
#include <uWebSockets/src/App.h>

namespace musicat::server::ws::player::events
{

void
drain (uws_ws_t *ws)
{
    const bool debug = get_debug_state ();

    if (debug)
        {
            fprintf (stderr, "[server DRAIN] %lu %u\n", (uintptr_t)ws,
                     ws->getBufferedAmount ());
        }
}

} // musicat::server::ws::player::events
