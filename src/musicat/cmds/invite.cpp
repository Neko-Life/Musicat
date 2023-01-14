#include "musicat/cmds.h"

namespace musicat
{
namespace command
{
namespace invite
{
dpp::slashcommand
get_register_obj (const dpp::snowflake &sha_id)
{
    return dpp::slashcommand ("invite", "My invite link", sha_id);
}

void
slash_run (const dpp::interaction_create_t &event)
{
    event.reply ("** [ ❤️ "
                 "](https://discord.com/api/oauth2/"
                 "authorize?client_id=960168583969767424&permissions="
                 "412353875008&scope=bot%20applications.commands) **");
}
}
}
}
