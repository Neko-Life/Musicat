#include "musicat/events/on_message_create.h"
#include "musicat/eliza.h"

namespace musicat::events
{

// Update channel last message ID
int
update_channel_last_message_id (const dpp::snowflake &channel_id, const dpp::snowflake &last_message_id)
{
    dpp::channel *c = dpp::find_channel (channel_id);
    if (!c) return -1;

    c->last_message_id = last_message_id;
    return 0;
}

void
on_message_create (dpp::cluster *client)
{
    client->on_message_create ([] (const dpp::message_create_t &event) {
        update_channel_last_message_id(event.msg.channel_id, event.msg.id);

        eliza::handle_message_create (event);
    });
}
} // musicat::events
