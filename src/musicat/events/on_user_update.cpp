#include "musicat/events/on_user_update.h"

namespace musicat::events
{
void
on_user_update (dpp::cluster *client)
{
    client->on_user_update ([] (const dpp::user_update_t &event) {
        std::cerr << "User update:\n" << event.raw_event << "\n";
    });
}
} // musicat::events
