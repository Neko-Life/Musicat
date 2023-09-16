#include "musicat/cmds.h"
#include "musicat/util.h"

namespace musicat
{
namespace command
{
namespace stop
{
dpp::slashcommand
get_register_obj (const dpp::snowflake &sha_id)
{
    return dpp::slashcommand ("stop", "STOP CURRENT PLAYBACK! STOP IT NOW!!!",
                              sha_id);
}

void
slash_run (const dpp::slashcommand_t &event)
{
    auto player_manager = get_player_manager_ptr ();
    if (!player_manager)
        {
            return;
        }

    auto p = player_manager->get_player (event.command.guild_id);
    dpp::voiceconn *v = event.from->get_voice (event.command.guild_id);

    if (util::is_player_not_playing (p, v))
        {
            event.reply ("I'm not playing anything");
            return;
        }

    if (!player_manager->voice_ready (event.command.guild_id, event.from,
                                      event.command.usr.id))
        {
            event.reply ("Please wait while I'm getting ready to stream");
            return;
        }

    try
        {
            auto vcu = get_voice_from_gid (event.command.guild_id,
                                           event.command.usr.id);
            if (vcu.first->id != v->channel_id)
                {
                    event.reply ("You're not in my voice channel!");
                    return;
                }
        }
    catch (...)
        {
            event.reply ("You're not in a voice channel!");
            return;
        }

    player_manager->stop_stream (event.command.guild_id);

    p->skip (v);
    p->set_stopped (true);
    v->voiceclient->pause_audio (true);

    player_manager->set_manually_paused (event.command.guild_id);

    event.reply ("Stopped");
}
} // stop
} // command
} // musicat
