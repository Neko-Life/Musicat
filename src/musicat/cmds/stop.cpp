// clang-format off
#include "musicat/player.h"
#include "musicat/player_manager.h"
// clang-format on

#include "musicat/cmds/stop.h"
#include "musicat/musicat.h"
#include "musicat/util.h"

namespace musicat::command::stop
{
dpp::slashcommand
get_register_obj (const dpp::snowflake &sha_id)
{
    return dpp::slashcommand ("stop", "STOP CURRENT PLAYBACK! STOP IT NOW!!!", sha_id);
}

void
slash_run (const dpp::slashcommand_t &event)
{
    auto p = player::manager::get_player (event.command.guild_id);
    dpp::voiceconn *v = event.from ()->get_voice (event.command.guild_id);

    if (util::is_player_not_playing (p, v))
        return event.reply ("I'm not playing anything");

    if (!player::manager::voice_ready (event.command.guild_id, event.from ()->shard_id, event.command.usr.id))
        return event.reply ("Please wait while I'm getting ready to stream");

    auto vcu = get_voice_from_gid (event.command.guild_id, event.command.usr.id);

    if (!vcu.first)
        return event.reply ("You're not in a voice channel!");

    if (vcu.first->id != v->channel_id)
        return event.reply ("You're not in my voice channel!");

    p->skip_playback (v);
    p->stopped = true;
    v->voiceclient->pause_audio (true);

    player::manager::set_manually_paused (event.command.guild_id);

    event.reply ("Stopped");

    player::manager::update_info_embed (event.command.guild_id, false);
}
} // musicat::command::stop
