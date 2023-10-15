#include "musicat/cmds/volume.h"
#include "musicat/cmds.h"
#include "musicat/musicat.h"
#include "musicat/util.h"

#define MIN_PERCENTAGE 1
#define MIN_PERCENTAGE_STR "1"

#define MAX_PERCENTAGE 500
#define MAX_PERCENTAGE_STR "500"

namespace musicat::command::volume
{
dpp::slashcommand
get_register_obj (const dpp::snowflake &sha_id)
{
    return dpp::slashcommand ("volume", "Set [playback] volume", sha_id)
        .add_option (dpp::command_option (
                         dpp::co_integer, "percentage",
                         "Volume percentage [to set]. <" MIN_PERCENTAGE_STR
                         "-" MAX_PERCENTAGE_STR ">",
                         true)
                         .set_min_value (MIN_PERCENTAGE)
                         .set_max_value (MAX_PERCENTAGE));
}

void
slash_run (const dpp::slashcommand_t &event)
{
    // perquisite
    player::player_manager_ptr_t player_manager = get_player_manager_ptr ();

    if (!player_manager)
        {
            return;
        }

    if (!player_manager->voice_ready (event.command.guild_id, event.from,
                                      event.command.usr.id))
        {
            event.reply ("Please wait while I'm getting ready to stream");
            return;
        }

    auto player = player_manager->get_player (event.command.guild_id);

    if (!player)
        {
            event.reply ("No active player in this guild");
            return;
        }

    // argument
    int64_t v_arg = -1;
    get_inter_param (event, "percentage", &v_arg);

    if (v_arg < MIN_PERCENTAGE || v_arg > MAX_PERCENTAGE)
        {
            event.reply ("Invalid `percentage` argument. Must be "
                         "between " MIN_PERCENTAGE_STR "% "
                         "and " MAX_PERCENTAGE_STR "% inclusive");
            return;
        }

    player->set_volume = v_arg;

    event.reply (std::string ("Setting volume to ") + std::to_string (v_arg)
                 + "%");
}

} // musicat::command::volume
