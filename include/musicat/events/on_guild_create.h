#ifndef MUSICAT_EVENTS_ON_GUILD_CREATE_H
#define MUSICAT_EVENTS_ON_GUILD_CREATE_H

#include <dpp/dpp.h>

namespace musicat::events
{

void on_guild_create (dpp::cluster *client);

} // musicat::events

#endif // MUSICAT_EVENTS_ON_GUILD_CREATE_H
