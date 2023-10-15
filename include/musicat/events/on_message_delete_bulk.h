#ifndef MUSICAT_EVENTS_ON_MESSAGE_DELETE_BULK_H
#define MUSICAT_EVENTS_ON_MESSAGE_DELETE_BULK_H

#include <dpp/dpp.h>

namespace musicat::events
{

void on_message_delete_bulk (dpp::cluster *client);

} // musicat::events

#endif // MUSICAT_EVENTS_ON_MESSAGE_DELETE_BULK_H
