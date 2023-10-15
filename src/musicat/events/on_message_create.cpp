#include "musicat/events/on_message_create.h"

namespace musicat::events
{
void
on_message_create (dpp::cluster *client)
{
    client->on_message_create ([] (const dpp::message_create_t &event) {
        // Update channel last message Id
        dpp::channel *c = dpp::find_channel (event.msg.channel_id);
        if (c)
            c->last_message_id = event.msg.id;
    });
}
} // musicat::events
