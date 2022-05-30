#include "musicat/cmds.h"

namespace musicat_command {
    namespace queue {
        dpp::slashcommand get_register_obj(const dpp::snowflake sha_id) {
            return dpp::slashcommand(
                "queue",
                "Show or modify [tracks in the] queue",
                sha_id
            ).add_option(
                dpp::command_option(
                    dpp::co_integer,
                    "action",
                    "Modify [tracks in the] queue"
                ).add_choice(
                    dpp::command_option_choice("Shuffle", queue_modify_t::m_shuffle)
                ).add_choice(
                    dpp::command_option_choice("Reverse", queue_modify_t::m_reverse)
                ).add_choice(
                    dpp::command_option_choice("Clear Left", queue_modify_t::m_clear_left)
                ).add_choice(
                    dpp::command_option_choice("Clear", queue_modify_t::m_clear)
                )
            );
        }

        void slash_run(const dpp::interaction_create_t& event, player_manager_ptr player_manager)
        {
            auto queue = player_manager->get_queue(event.command.guild_id);
            if (queue.empty())
            {
                event.reply("No track");
                return;
            }

            dpp::message msg;
            dpp::embed embed;
            // std::vector<string> queue_str;
            string desc = "";
            int64_t id = 1;

            for (auto i = queue.begin(); i != queue.end(); i++)
            {
                if (i == queue.begin())
                {
                    desc += "Current track: [" + i->title() + "](" + i->url() + ") - <@" + std::to_string(i->user_id) + ">\n\n";
                }
                else
                {
                    desc += std::to_string(id) + ": [" + i->title() + "](" + i->url() + ") - <@" + std::to_string(i->user_id) + ">\n";
                    id++;
                }
            }

            embed.set_title("Queue")
                .set_description(desc.length() > 2048 ? "Description too long, pagination is on the way!" : desc);
            msg.add_embed(embed);
            event.reply(msg);
        }
    }
}
