#include "musicat/pagination.h"
#include "musicat/player_manager.h"

#include "musicat/events/on_message_delete.h"

namespace musicat::events
{
void
on_message_delete (dpp::cluster *client)
{
    client->on_message_delete (
        [] (const dpp::message_delete_t &event)
            {
                player::manager::handle_on_message_delete (event);
                paginate::handle_on_message_delete (event);
            });
}
} // musicat::events
