#include "musicat/cmds.h"

namespace musicat_command {
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
            // auto p = player_manager->get_player;
            // int64_t fr = 0;
            // int64_t to = 0;
            // mc::get_inter_param(event, "track", &fr);
            // mc::get_inter_param(event, "amount", &to);

            // if (fr < 1) fr = 1;
            // if (fr > p->queue.size()) {}
        }
    }
}
