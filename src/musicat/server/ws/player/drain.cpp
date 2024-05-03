#include "musicat/musicat.h"
#include "musicat/server/ws/player.h"
#ifndef MUSICAT_NO_SERVER
#include <uWebSockets/src/App.h>
#endif

namespace musicat::server::ws::player::events
{

#ifndef MUSICAT_NO_SERVER
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
#endif

} // musicat::server::ws::player::events
