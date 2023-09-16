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
slash_run (const dpp::slashcommand_t &event)
{
    auto player_manager = get_player_manager_ptr ();
    if (!player_manager)
        {
            return;
        }

    int64_t arg_state = -1;
    int64_t arg_no_duplicate_threashold = -1;
    get_inter_param (event, "state", &arg_state);
    get_inter_param (event, "no-duplicate-threshold",
                     &arg_no_duplicate_threashold);

    auto guild_player = player_manager->create_player (event.command.guild_id);
    guild_player->from = event.from;

    if (guild_player->saved_config_loaded != true)
        player_manager->load_guild_player_config (event.command.guild_id);
    bool player_autoplay = guild_player->auto_play;
    size_t st = guild_player->max_history_size;
    std::string reply = "";
    if (arg_state > -1)
        {
            reply += std::string ("Autoplay ")
                     + (guild_player->set_auto_play (arg_state ? true : false)
                                .auto_play
                            ? "enabled, add a track to initialize "
                              "autoplay "
                              "playlist"
                            : "disabled")
                     + "\n";
        }
    else
        reply += std::string ("Autoplay is currently ")
                 + (guild_player->auto_play ? "enabled" : "disabled") + "\n";

    if (arg_no_duplicate_threashold > -1)
        {
            if (arg_no_duplicate_threashold > 1000)
                arg_no_duplicate_threashold = 1000;
            guild_player->set_max_history_size (arg_no_duplicate_threashold);
            reply += "Set No-Duplicate Threshold to "
                     + std::to_string (arg_no_duplicate_threashold);
        }
    else
        {
            reply += "No-Duplicate Threshold is " + std::to_string (st);
        }
    event.reply (reply);
    if (player_autoplay != guild_player->auto_play
        || (guild_player->auto_play && st != guild_player->max_history_size))
        try
            {
                player_manager->update_info_embed (event.command.guild_id);
            }
        catch (...)
            {
            }
}
} // autoplay
} // command
} // musicat
