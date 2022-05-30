#include "musicat/cmds.h"

namespace musicat_command {
    namespace skip {
        dpp::slashcommand get_register_obj(const dpp::snowflake sha_id) {
            return dpp::slashcommand(
                "skip",
                "Skip [currently playing] song",
                sha_id
            ).add_option(
                dpp::command_option(
                    dpp::co_integer,
                    "amount",
                    "How many [song] to skip"
                )
            );
        }

        void slash_run(const dpp::interaction_create_t& event, player_manager_ptr player_manager) {
            if (!player_manager->voice_ready(event.command.guild_id))
            {
                event.reply("Please wait while I'm getting ready to stream");
                return;
            }
            try
            {
                auto v = event.from->get_voice(event.command.guild_id);
                if (player_manager->skip(v, event.command.guild_id, event.command.usr.id))
                {
                    event.reply("Skipped");
                }
                else event.reply("I'm not playing anything");
            }
            catch (const mc::exception& e)
            {
                event.reply(e.what());
            }
        }
    }
}
