#ifndef MUSICAT_EVENTS_ON_MESSAGE_CREATE_H
#define MUSICAT_EVENTS_ON_MESSAGE_CREATE_H

#include <dpp/dpp.h>

namespace musicat::events
{

void on_message_create (dpp::cluster *client);

} // musicat::events

#endif // MUSICAT_EVENTS_ON_MESSAGE_CREATE_H
