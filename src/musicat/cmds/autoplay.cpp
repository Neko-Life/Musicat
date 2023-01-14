#include "musicat/cmds.h"

namespace musicat
{
namespace command
{
namespace autoplay
{
dpp::slashcommand
get_register_obj (const dpp::snowflake &sha_id)
{
    return dpp::slashcommand ("autoplay",
                              "Set [guild player] autoplay [state]", sha_id)
        .add_option (
            dpp::command_option (dpp::co_integer, "state",
                                 "Set [to this] state", false)
                .add_choice (dpp::command_option_choice ("Enable", 1))
                .add_choice (dpp::command_option_choice ("Disable", 0)))
        .add_option (
            dpp::command_option (dpp::co_integer, "no-duplicate-threshold",
                                 "Number of duplicate guard, max 1000"));
}

void
slash_run (const dpp::interaction_create_t &event,
           player::player_manager_ptr player_manager)
{
    int64_t a = -1;
    int64_t b = -1;
    get_inter_param (event, "state", &a);
    get_inter_param (event, "no-duplicate-threshold", &b);
    auto g = player_manager->create_player (event.command.guild_id);
    if (g->saved_config_loaded != true)
        player_manager->load_guild_player_config (event.command.guild_id);
    bool c = g->auto_play;
    size_t st = g->max_history_size;
    std::string reply = "";
    if (a > -1)
        {
            reply += std::string ("Autoplay ")
                     + (g->set_auto_play (a ? true : false).auto_play
                            ? "enabled, add a track to initialize autoplay "
                              "playlist"
                            : "disabled")
                     + "\n";
        }
    else
        reply += std::string ("Autoplay is currently ")
                 + (g->auto_play ? "enabled" : "disabled") + "\n";

    if (b > -1)
        {
            if (b > 1000)
                b = 1000;
            g->set_max_history_size (b);
            reply += "Set No-Duplicate Threshold to " + std::to_string (b);
        }
    else
        {
            reply += "No-Duplicate Threshold is " + std::to_string (st);
        }
    event.reply (reply);
    if (c != g->auto_play || (g->auto_play && st != g->max_history_size))
        try
            {
                player_manager->update_info_embed (event.command.guild_id);
            }
        catch (...)
            {
            }
}
}
}
}
