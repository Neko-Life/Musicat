#include "musicat/cmds.h"
#include "musicat/musicat.h"
#include "musicat/player.h"
#include <dpp/dpp.h>
#include <mutex>

namespace musicat
{
namespace command
{
namespace join
{
dpp::slashcommand
get_register_obj (const dpp::snowflake &sha_id)
{
    return dpp::slashcommand ("join", "Join [your voice channel]", sha_id);
}

void
slash_run (const dpp::interaction_create_t &event,
           player::player_manager_ptr player_manager)
{
    auto p = player_manager->create_player (event.command.guild_id);
    if (!p->channel_id)
        p->set_channel (event.command.channel_id);

    int res = join_voice (event.from, player_manager, event.command.guild_id,
                          event.command.usr.id, event.from->creator->me.id);

    switch (res)
        {
        case 0:
            event.reply ("Joining...");
            return;
        case 1:
            event.reply ("Join a voice channel first you dummy");
            return;
        case 2:
            event.reply ("I'm already in a voice channel");
            return;
        case 3:
            event.reply ("`[ERROR]` No channel to join");
            return;
        case 4:
            event.reply ("I have no permission to join your voice channel");
            return;
        }
}
}

namespace leave
{
dpp::slashcommand
get_register_obj (const dpp::snowflake &sha_id)
{
    return dpp::slashcommand ("leave", "Leave [your voice channel]", sha_id);
}

void
slash_run (const dpp::interaction_create_t &event,
           player::player_manager_ptr player_manager)
{
    std::pair<dpp::channel *, std::map<dpp::snowflake, dpp::voicestate> > usc,
        vcc;

    // user is in a voice
    bool has_usc = false;
    // client is in a voice
    bool has_vcc = false;

    try
        {
            usc = get_voice_from_gid (event.command.guild_id,
                                      event.command.usr.id);
            has_usc = true;

            vcc = get_voice_from_gid (event.command.guild_id,
                                      event.from->creator->me.id);
            has_vcc = true;

            if (usc.first && usc.first->id)
                {
                    if (vcc.first && vcc.first->id
                        && usc.first->id != vcc.first->id)
                        {
                            event.reply ("You're not in my voice channel");
                            return;
                        }

                    std::lock_guard<std::mutex> lk (player_manager->dc_m);
                    player_manager->disconnecting.insert_or_assign (
                        event.command.guild_id, usc.first->id);
                    event.from->disconnect_voice (event.command.guild_id);
                    event.reply ("Leaving...");
                    return;
                }
        }
    catch (...)
        {
            if (!has_usc)
                {
                    event.reply ("You're not in a voice channel");
                    return;
                }
            if (!has_vcc)
                {
                    event.reply ("I'm not in a voice channel");
                    return;
                }

            event.reply ("`[FATAL]` UNKNOWN ERROR");
            return;
        }

    event.reply ("`[ERROR]` Missing channel");
}
}
}
}
