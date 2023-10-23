#include "musicat/events/on_message_delete.h"
#include "musicat/musicat.h"
#include "musicat/pagination.h"

namespace musicat::events
{
void
on_message_delete (dpp::cluster *client)
{
    client->on_message_delete ([] (const dpp::message_delete_t &event) {
        auto player_manager = get_player_manager_ptr ();

        if (player_manager)
            player_manager->handle_on_message_delete (event);

        paginate::handle_on_message_delete (event);
    });
}
} // musicat::events
