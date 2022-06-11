#include "musicat/cmds.h"

namespace musicat_command {
    namespace autoplay {
        dpp::slashcommand get_register_obj(const dpp::snowflake sha_id) {
            return dpp::slashcommand("autoplay", "Set [guild player] autoplay [state]", sha_id)
                .add_option(
                    dpp::command_option(
                        dpp::co_integer,
                        "state",
                        "Set [to this] state",
                        true
                    ).add_choice(
                        dpp::command_option_choice("Enable", 1)
                    ).add_choice(
                        dpp::command_option_choice("Disable", 0)
                    )
                );
        }

        void slash_run(const dpp::interaction_create_t& event, player_manager_ptr player_manager) {
            int64_t a = 0;
            mc::get_inter_param(event, "state", &a);
            auto g = player_manager->create_player(event.command.guild_id);
            bool b = g->auto_play;
            event.reply(string("Autoplay ")
                + (
                    g->set_auto_play(a ? true : false).auto_play
                    ? "enabled, add a track to initialize autoplay playlist"
                    : "disabled"
                    )
            );
            if (b != g->auto_play) try
            {
                player_manager->update_info_embed(event.command.guild_id);
            }
            catch (...) {}
        }
    }
}
