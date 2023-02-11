#include <musicat/cmds.h>

namespace musicat
{
namespace command
{
namespace seek
{
dpp::slashcommand
get_register_obj (const dpp::snowflake &sha_id)
{
    return dpp::slashcommand ("seek",
                              "Seek [currently playing] track",
                              sha_id)
        .add_option (
            dpp::command_option (dpp::co_string, "to",
                                 "Format: Absolute `[hour]:[minute]:second`. Example: 4:20",
                                 true));
}

void
slash_run (const dpp::interaction_create_t &event,
                player::player_manager_ptr player_manager)
{
    event.reply("This command is still in development");
    return;
    std::string arg_to = "";
    get_inter_param (event, "to", &arg_to);
}

} // seek
} // command
} // musicat
