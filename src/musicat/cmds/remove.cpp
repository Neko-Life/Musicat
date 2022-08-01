#include "musicat/cmds.h"

namespace musicat {
    namespace command {
        namespace remove {
            dpp::slashcommand get_register_obj(const dpp::snowflake sha_id) {
                return dpp::slashcommand("remove", "Remove [tracks] from the queue", sha_id)
                    .add_option(
                        dpp::command_option(
                            dpp::co_integer,
                            "track",
                            "Remove [this] track",
                            true
                        )
                    ).add_option(
                        dpp::command_option(
                            dpp::co_integer,
                            "amount",
                            "Amount [of track] to remove [starting from this track]"
                        )
                    );
            }

            void slash_run(const dpp::interaction_create_t& event, player_manager_ptr player_manager) {
                int64_t fr = 1;
                int64_t to = 1;
                musicat::get_inter_param(event, "track", &fr);
                musicat::get_inter_param(event, "amount", &to);
                if (fr < 1 || ((size_t)fr + 1) >player_manager->get_queue(event.command.guild_id).size())
                {
                    event.reply("No track in position " + std::to_string(fr));
                    return;
                }

                if (to < 1) to = 1;
                size_t ret = player_manager->remove_track(event.command.guild_id, (size_t)fr, (size_t)to);
                if (ret)
                {
                    event.reply("Removed " + std::to_string(ret) + " track" + string(ret > 1 ? "s" : ""));
                }
                else
                {
                    event.reply("No track was removed");
                }
            }
        }
    }
}
