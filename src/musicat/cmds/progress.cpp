#include "dispatcher.h"
#include "musicat/cmds.h"
#include "musicat/musicat.h"
#include "musicat/player.h"

namespace musicat
{
namespace command
{
namespace progress
{
dpp::slashcommand
get_register_obj (const dpp::snowflake &sha_id)
{
    return dpp::slashcommand ("progress", "Current [track] progress",
                              sha_id);
}

dpp::message _reply_embed (player::MCTrack &current_track, const int64_t &current_ms, const int64_t &duration)
{
    dpp::message msg;

    dpp::embed embed;

    embed.set_title (current_track.title ())
        .set_url (current_track.url ())
        .set_description (std::string ("[") + format_duration (current_ms) + "/" + format_duration (duration) + "]");

    msg.add_embed(embed);

    msg.add_component(
        dpp::component ()
            .add_component (dpp::component ()
                                .set_label("Update")
                                .set_id ("progress/u")
                                .set_type (dpp::cot_button)
                                .set_style (dpp::cos_primary))
    );

    return msg;
}

struct _processed_t {
    player::MCTrack current_track;
    int64_t current_ms;
    int64_t duration;
    bool error;
    std::string error_msg;
};

_processed_t
_create_processed_t (std::string error_msg = "")
{
    return { {}, 0, 0, error_msg.length () ? true : false, error_msg };
}

_processed_t
_process_command (const dpp::interaction_create_t &event,
           player::player_manager_ptr player_manager)
{
    auto guild_player = player_manager->get_player(event.command.guild_id);
    if (!guild_player) return _create_processed_t ("I'm not playing anything");

    if (!util::player_has_current_track (guild_player))
    {
        return _create_processed_t ("Not playing anything");
    }

    player::MCTrack current_track = guild_player->current_track;
    const int64_t duration = current_track.info.duration ();

    if (!duration) return _create_processed_t ("`[ERROR]` Missing metadata");

    float byte_per_ms = (float)current_track.filesize / (float)duration;

    const int64_t current_ms = current_track.current_byte && byte_per_ms
        ? (float)current_track.current_byte / byte_per_ms
        : 0;

    return { current_track, current_ms, duration, false, "" };
}

void
slash_run (const dpp::slashcommand_t &event,
           player::player_manager_ptr player_manager)
{
    if (!player_manager->voice_ready (event.command.guild_id))
        {
            event.reply ("Please wait while I'm getting ready to stream");
            return;
        }
    try
        {
            _processed_t value = _process_command (event, player_manager);

            if (value.error)
                return event.reply (value.error_msg);

            event.reply(_reply_embed (value.current_track, value.current_ms, value.duration));
        }
    catch (const exception &e)
        {
            event.reply (e.what ());
        }
}

void
update_progress (const dpp::button_click_t &event,
           player::player_manager_ptr player_manager)
{
    try
        {
            _processed_t value = _process_command (event, player_manager);

            if (value.error)
                return event.reply (value.error_msg);

            dpp::message *reply_embed = new dpp::message (_reply_embed (value.current_track, value.current_ms, value.duration));
            dpp::interaction_response reply;

            reply.type = dpp::ir_update_message;
            reply.msg = reply_embed;

            event.from->creator->interaction_response_create(event.command.id, event.command.token, reply,
                [reply_embed] (const dpp::confirmation_callback_t &ccb) {
                    delete reply_embed;
                    auto cb = dpp::utility::log_error();
                    cb (ccb);
                });
        }
    catch (const exception &e)
        {
            event.reply (e.what ());
        }
}

} // progress
} // command
} // musicat
