#include "musicat/cmds/progress.h"
#include "musicat/cmds.h"
#include "musicat/mctrack.h"
#include "musicat/musicat.h"

namespace musicat::command::progress
{
dpp::slashcommand
get_register_obj (const dpp::snowflake &sha_id)
{
    return dpp::slashcommand ("progress", "Current [track] progress", sha_id);
}

dpp::message
_reply_embed (player::MCTrack &current_track, const int64_t &current_ms,
              const int64_t &duration)
{
    dpp::message msg;

    dpp::embed embed;

    embed.set_title (mctrack::get_title (current_track))
        .set_url (mctrack::get_url (current_track))
        .set_description (std::string ("[") + format_duration (current_ms)
                          + "/" + format_duration (duration) + "]");

    msg.add_embed (embed);

    msg.add_component (
        dpp::component ().add_component (dpp::component ()
                                             .set_label ("Update")
                                             .set_id ("progress/u")
                                             .set_type (dpp::cot_button)
                                             .set_style (dpp::cos_primary)));

    return msg;
}

struct _processed_t
{
    player::MCTrack current_track;
    int64_t current_ms;
    int64_t duration;
    bool error;
    std::string error_msg;
};

_processed_t
_create_processed_t (std::string error_msg = "")
{
    return { {}, 0, 0, !error_msg.empty (), error_msg };
}

_processed_t
_process_command (const dpp::interaction_create_t &event,
                  player::player_manager_ptr_t player_manager)
{
    auto guild_player = player_manager->get_player (event.command.guild_id);
    if (!guild_player)
        return _create_processed_t ("I'm not playing anything");

    if (!util::player_has_current_track (guild_player))
        {
            return _create_processed_t ("Not playing anything");
        }

    player::MCTrack current_track = guild_player->current_track;
    player::track_progress prog = util::get_track_progress (current_track);

    if (prog.status)
        return _create_processed_t ("`[ERROR]` Missing metadata");

    return { current_track, prog.current_ms, prog.duration, false, "" };
}

void
slash_run (const dpp::slashcommand_t &event)
{
    auto player_manager = cmd_pre_get_player_manager_ready (event);
    if (player_manager == NULL)
        return;

    try
        {
            _processed_t value = _process_command (event, player_manager);

            if (value.error)
                return event.reply (value.error_msg);

            event.reply (_reply_embed (value.current_track, value.current_ms,
                                       value.duration));
        }
    catch (const exception &e)
        {
            event.reply (e.what ());
        }
}

void
update_progress (const dpp::button_click_t &event)
{
    try
        {
            auto player_manager = get_player_manager_ptr ();
            if (!player_manager)
                {
                    return;
                }

            _processed_t value = _process_command (event, player_manager);

            if (value.error)
                return event.reply (value.error_msg);

            dpp::interaction_response reply (dpp::ir_update_message,
                                             _reply_embed (value.current_track,
                                                           value.current_ms,
                                                           value.duration));

            event.from->creator->interaction_response_create (
                event.command.id, event.command.token, reply);
        }
    catch (const exception &e)
        {
            event.reply (e.what ());
        }
}

} // musicat::command::progress
