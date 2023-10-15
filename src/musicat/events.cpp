#include "musicat/events/on_autocomplete.h"
#include "musicat/events/on_button_click.h"
#include "musicat/events/on_channel_update.h"
#include "musicat/events/on_form_submit.h"
#include "musicat/events/on_message_create.h"
#include "musicat/events/on_message_delete.h"
#include "musicat/events/on_message_delete_bulk.h"
#include "musicat/events/on_ready.h"
#include "musicat/events/on_slashcommand.h"
#include "musicat/events/on_voice_ready.h"
#include "musicat/events/on_voice_state_update.h"
#include "musicat/events/on_voice_track_marker.h"
#include "musicat/musicat.h"

namespace musicat::events
{
int
load_events ()
{
    auto client = get_client_ptr ();

    on_ready (client);
    on_message_create (client);
    on_button_click (client);
    on_form_submit (client);
    on_autocomplete (client);
    on_slashcommand (client);
    on_voice_ready (client);
    on_voice_state_update (client);
    on_voice_track_marker (client);
    on_message_delete (client);
    on_message_delete_bulk (client);
    on_channel_update (client);

    return 0;
}

} // musicat::events
