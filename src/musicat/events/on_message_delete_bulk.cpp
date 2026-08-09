#include "musicat/pagination.h"
#include "musicat/player_manager.h"

#include "musicat/events/on_message_delete_bulk.h"

namespace musicat::events
{
void
on_message_delete_bulk (dpp::cluster *client)
{
    client->on_message_delete_bulk (
        [] (const dpp::message_delete_bulk_t &event)
            {
                player::manager::handle_on_message_delete_bulk (event);

                paginate::handle_on_message_delete_bulk (event);
            });
}
} // musicat::events
