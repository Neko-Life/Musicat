#include "musicat/events/on_autocomplete.h"
#include "musicat/events/on_button_click.h"
#include "musicat/events/on_channel_update.h"
#include "musicat/events/on_form_submit.h"
#include "musicat/events/on_guild_create.h"
#include "musicat/events/on_guild_delete.h"
#include "musicat/events/on_message_create.h"
#include "musicat/events/on_message_delete.h"
#include "musicat/events/on_message_delete_bulk.h"
#include "musicat/events/on_ready.h"
#include "musicat/events/on_slashcommand.h"
#include "musicat/events/on_user_update.h"
#include "musicat/events/on_voice_ready.h"
#include "musicat/events/on_voice_server_update.h"
#include "musicat/events/on_voice_state_update.h"
#include "musicat/events/on_voice_track_marker.h"

namespace musicat::events
{
int
load_events (dpp::cluster *client)
{
    on_ready (client);
    on_message_create (client);
    on_button_click (client);
    on_form_submit (client);
    on_autocomplete (client);
    on_slashcommand (client);
    on_voice_ready (client);
    on_voice_state_update (client);
    on_voice_track_marker (client);
    on_voice_server_update (client);
    on_message_delete (client);
    on_message_delete_bulk (client);
    on_channel_update (client);
    on_guild_create (client);
    on_guild_delete (client);
    on_user_update (client);

    return 0;
}

} // musicat::events
