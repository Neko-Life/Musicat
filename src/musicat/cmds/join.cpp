// clang-format off
#include "musicat/player.h"
#include "musicat/player_manager.h"
// clang-format on

#include "musicat/cmds/join.h"
#include "musicat/musicat.h"

namespace musicat::command
{
namespace join
{
dpp::slashcommand
get_register_obj (const dpp::snowflake &sha_id)
{
    return dpp::slashcommand ("join", "Join [your voice channel]", sha_id);
}

int
run (const dpp::slashcommand_t &event, std::string &out)
{
    auto guild_player = player::manager::create_player (event.command.guild_id);
    if (!guild_player)
        {
            out = "`[ERROR]` Failed creating guild player";
            return 1;
        }

    if (!guild_player->text_channel_id)
        guild_player->set_channel (event.command.channel_id);

    int res = join_voice (event.from (), event.command.guild_id, event.command.usr.id, event.from ()->creator->me.id);

    std::string msg;
    switch (res)
        {
        case 0:
            out = "Joining...";
            guild_player->set_channel (event.command.channel_id);
            break;
        case 1:
            out = "Join a voice channel first you dummy";
            break;
        case 2:
            out = "I'm already in a voice channel";
            break;
        case 3:
            out = "`[ERROR]` No channel to join";
            break;
        case 4:
            out = "I have no permission to join your voice channel";
            break;
        default:
            out = "`[ERROR]` Unknown status code: " + std::to_string (res);
        }

    return res;
}

void
slash_run (const dpp::slashcommand_t &event)
{
    std::string out;
    run (event, out);
    if (!out.empty ())
        event.reply (out);
}
} // join

namespace leave
{
dpp::slashcommand
get_register_obj (const dpp::snowflake &sha_id)
{
    return dpp::slashcommand ("leave", "Leave [your voice channel]", sha_id);
}

void
slash_run (const dpp::slashcommand_t &event)
{
    std::pair<dpp::channel *, std::map<dpp::snowflake, dpp::voicestate> > usc, vcc;

    usc = get_voice_from_gid (event.command.guild_id, event.command.usr.id);
    if (!usc.first)
        {
            event.reply ("You're not in a voice channel");
            return;
        }

    vcc = get_voice_from_gid (event.command.guild_id, event.from ()->creator->me.id);
    if (!vcc.first)
        {
            event.reply ("I'm not in a voice channel");
            return;
        }

    if (vcc.first && vcc.first->id && usc.first->id != vcc.first->id)
        {
            event.reply ("You're not in my voice channel");
            return;
        }

    player::manager::set_disconnecting (event.command.guild_id, usc.first->id);

    player::manager::disconnect_voice (event.from (), event.command.guild_id);
    event.reply ("Leaving...");
}
} // leave

} // musicat::command
