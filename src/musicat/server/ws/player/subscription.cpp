#include "musicat/musicat.h"
#include "musicat/server/ws/player.h"
#ifndef MUSICAT_NO_SERVER
#include <uWebSockets/src/App.h>
#endif

namespace musicat::server::ws::player::events
{
#ifndef MUSICAT_NO_SERVER

void
subscription (uws_ws_t *ws, std::string_view topic, int idk1, int idk2)
{
    const bool debug = get_debug_state ();

    if (debug)
        {
            fprintf (stderr, "[server SUBSCRIPTION] %lu, %d, %d: %s\n",
                     (uintptr_t)ws, idk1, idk2, std::string (topic).c_str ());
        }
}

#endif
} // musicat::server::ws::player::events
