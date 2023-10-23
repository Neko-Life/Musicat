#include "musicat/cmds/hello.h"

namespace musicat::command::hello
{
dpp::slashcommand
get_register_obj (const dpp::snowflake &sha_id)
{
    return dpp::slashcommand ("hello", "Hello World!", sha_id);
}

void
slash_run (const dpp::slashcommand_t &event)
{
    event.reply ("Hello there!!");
}
} // musicat::command::hello
