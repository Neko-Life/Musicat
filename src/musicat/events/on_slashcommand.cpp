#include "musicat/events/on_slashcommand.h"
#include "musicat/cmds.h"
#include "musicat/slash.h"

namespace musicat::events
{
inline const command::command_handler_t *command_handlers
    = command::get_slash_command_handlers ();

void
on_slashcommand (dpp::cluster *client)
{
    client->on_slashcommand ([] (const dpp::slashcommand_t &event) {
        if (!event.command.guild_id)
            return;

        const std::string cmd = event.command.get_command_name ();

        auto status
            = command::handle_command ({ cmd, command_handlers, event });

        // if (cmd == "why")
        //     event.reply ("Why not");
        // else if (cmd == "repo")
        //     event.reply ("https://github.com/Neko-Life/Musicat");

        if (status == command::HANDLE_SLASH_COMMAND_NO_HANDLER)
            {
                event.reply (
                    "Seems like somethin's wrong here, I can't find that "
                    "command anywhere in my database");
            }
    });
}
} // musicat::events
