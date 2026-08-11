#include "musicat/player.h"

#include "musicat/cmds.h"
#include "musicat/cmds/pause.h"
#include "musicat/musicat.h"

namespace musicat::command::pause
{
dpp::slashcommand
get_register_obj (const dpp::snowflake &sha_id)
{
    return dpp::slashcommand ("pause", "Pause [currently playing] song", sha_id);
}

int
run (dpp::discord_client *from, const dpp::snowflake &user_id, const dpp::snowflake &guild_id, std::string &out, bool update_embed = true)
{
    auto pm_res = cmd_pre_get_player_manager_ready_werr (guild_id);
    if (pm_res == 1)
        {
            out = "Please wait while I'm getting ready to stream";
            return 1;
        }

    auto guild_player = player::manager::get_player (guild_id);
    if (guild_player && !guild_player->user_in_the_same_vc (user_id))
        {
            out = "You're not in my voice channel!";
            return 1;
        }

    if (player::manager::pause (guild_id, user_id, update_embed))
        out = "Paused";
    else
        out = "I'm not playing anything";

    return 1;
}

void
slash_run (const dpp::slashcommand_t &event)
{
    std::string out;
    int status = run (event.from (), event.command.usr.id, event.command.guild_id, out);

    if (status == 1)
        event.reply (out);
}

void
button_run (const dpp::button_click_t &event)
{
    std::string out;
    run (event.from (), event.command.usr.id, event.command.guild_id, out, false);

    player::manager::update_info_embed (event.command.guild_id, false, &event);
}

} // musicat::command::pause
