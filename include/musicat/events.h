#ifndef MUSICAT_EVENTS_H
#define MUSICAT_EVENTS_H

#include <dpp/dpp.h>

// #define USE_VOICE_SERVER_UPDATE_RECONNECT

namespace musicat::events
{

int load_events (dpp::cluster *client);

} // musicat::events

#endif // MUSICAT_EVENTS_H
