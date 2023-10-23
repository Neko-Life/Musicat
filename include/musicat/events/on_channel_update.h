#ifndef MUSICAT_EVENTS_ON_CHANNEL_UPDATE_H
#define MUSICAT_EVENTS_ON_CHANNEL_UPDATE_H

#include <dpp/dpp.h>

namespace musicat::events
{

void on_channel_update (dpp::cluster *client);

} // musicat::events

#endif // MUSICAT_EVENTS_ON_CHANNEL_UPDATE_H
