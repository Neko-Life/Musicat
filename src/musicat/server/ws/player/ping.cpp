#include "musicat/musicat.h"
#include "musicat/server/ws/player.h"
#ifndef MUSICAT_NO_SERVER
#include <uWebSockets/src/App.h>
#endif

namespace musicat::server::ws::player::events
{

#ifndef MUSICAT_NO_SERVER
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
#endif

} // musicat::server::ws::player::events
