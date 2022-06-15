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
                        false
                    ).add_choice(
                        dpp::command_option_choice("Enable", 1)
                    ).add_choice(
                        dpp::command_option_choice("Disable", 0)
                    )
                ).add_option(
                    dpp::command_option(
                        dpp::co_integer,
                        "no-duplicate-threshold",
                        "Number of duplicate guard, max 10000",
                        false
                    )
                );
        }

        void slash_run(const dpp::interaction_create_t& event, player_manager_ptr player_manager) {
            int64_t a = -1;
            int64_t b = -1;
            mc::get_inter_param(event, "state", &a);
            mc::get_inter_param(event, "no-duplicate-threshold", &b);
            auto g = player_manager->create_player(event.command.guild_id);
            bool c = g->auto_play;
            size_t st = g->max_history_size;
            string reply = "";
            if (a > -1)
            {
                reply += string("Autoplay ")
                    + (
                        g->set_auto_play(a ? true : false).auto_play
                        ? "enabled, add a track to initialize autoplay playlist"
                        : "disabled"
                        )
                    + "\n";
            }
            else reply += string("Autoplay is currently ") + (g->auto_play ? "enabled" : "disabled") + "\n";
            if (b > -1)
            {
                if (b > 10000) b = 10000;
                g->set_max_history_size(b);
                reply += "Set No Duplicate threshold to " + std::to_string(b);
            }
            event.reply(reply);
            if (c != g->auto_play || (g->auto_play && st != g->max_history_size)) try
            {
                player_manager->update_info_embed(event.command.guild_id);
            }
            catch (...) {}
        }
    }
}
