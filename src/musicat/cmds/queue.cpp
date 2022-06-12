#include "musicat/cmds.h"

namespace musicat_command {
    namespace queue {
        pages_t::pages_t() {
            this->client = NULL;
            this->message = {};
            this->pages = {};
            this->current = 0;
        }

        pages_t::pages_t(dpp::cluster* client, std::shared_ptr<dpp::message> message, std::vector<dpp::embed> pages, size_t current) {
            this->client = client;
            this->message = message;
            this->pages = pages;
            this->current = current;
        }

        pages_t::~pages_t() = default;

        void pages_t::edit_cb(const dpp::confirmation_callback_t& cb, size_t new_current) {
            if (cb.is_error())
            {
                fprintf(stderr, "[ERROR(player.668)] message_create callback error:\nmes: %s\ncode: %d\nerr:\n", cb.get_error().message.c_str(), cb.get_error().code);
                for (const auto& i : cb.get_error().errors)
                    fprintf(stderr, "c: %s\nf: %s\no: %s\nr: %s\n",
                        i.code.c_str(),
                        i.field.c_str(),
                        i.object.c_str(),
                        i.reason.c_str()
                    );
                return;
            }
            if (cb.value.index())
            {
                this->message = std::make_shared<dpp::message>(std::get<dpp::message>(cb.value));
                this->current = new_current;
            }
            else printf("No edit_cb size\n");
        }

        void pages_t::edit(size_t c) {
            if (this->current == c)
            {
                printf("Return pages_t::edit (current == c)\n");
                return;
            }
            dpp::embed to = this->pages.at(c);
            this->message->embeds.clear();
            this->message->add_embed(to);
            this->client->message_edit(*this->message, [this, c](const dpp::confirmation_callback_t& cb) {
                this->edit_cb(cb, c);
            });
        }

        void pages_t::next() {
            size_t c = this->current;
            if (c == (this->pages.size() - 1))
            {
                c = 0;
            }
            else c++;
            this->edit(c);
        }

        void pages_t::previous() {
            size_t c = this->current;
            if (c == 0)
            {
                c = this->pages.size() - 1;
            }
            else c--;
            this->edit(c);
        }

        void pages_t::home() {
            if (this->current == 0) return;
            this->edit(0);
        }

        std::map<dpp::snowflake, pages_t> paginated_messages = {};

        void update_page(dpp::snowflake msg_id, string param) {
            auto a = paginated_messages.find(msg_id);
            if (a == paginated_messages.end()) return;
            auto& b = a->second;
            if (param == "n")
            {
                b.next();
            }
            else if (param == "p")
            {
                b.previous();
            }
            else if (param == "h")
            {
                b.home();
            }
        }

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

            std::vector<dpp::embed> embeds = {};

            dpp::embed embed;
            // std::vector<string> queue_str;
            string desc = "";
            size_t id = 1;
            size_t qs = queue.size();
            size_t count = 0;

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
                    count++;
                }
                if (count && (!(count % 10) || id == qs))
                {
                    embed.set_title("Queue")
                        .set_description(desc.length() > 2048 ? "Description too long, pagination is on the way!" : desc);
                    embeds.emplace_back(embed);
                    embed = dpp::embed();
                    desc = "";
                    count = 0;
                }
            }

            dpp::message msg;
            msg.add_embed(embeds.front());
            bool paginate = embeds.size() > 1;
            if (paginate)
            {
                msg.add_component(
                    dpp::component().add_component(
                        dpp::component()
                        .set_emoji(u8"◀️")
                        .set_id("page_queue/p")
                        .set_type(dpp::cot_button)
                        .set_style(dpp::cos_primary)
                    ).add_component(
                        dpp::component()
                        .set_emoji(u8"🏠")
                        .set_id("page_queue/h")
                        .set_type(dpp::cot_button)
                        .set_style(dpp::cos_primary)
                    ).add_component(
                        dpp::component()
                        .set_emoji(u8"▶️")
                        .set_id("page_queue/n")
                        .set_type(dpp::cot_button)
                        .set_style(dpp::cos_primary)
                    )
                );
            }

            event.reply(msg, [event, paginate, player_manager, embeds](const dpp::confirmation_callback_t& cb) {
                if (cb.is_error())
                {
                    fprintf(stderr, "ERROR event_reply page_queue: %s\n", cb.get_error().message.c_str());
                    return;
                }
                if (paginate)
                {
                    event.get_original_response([player_manager, embeds](const dpp::confirmation_callback_t& cb2) {
                        if (cb2.is_error())
                        {
                            fprintf(stderr, "ERROR get_original_response page_queue: %s\n", cb2.get_error().message.c_str());
                            return;
                        }
                        std::shared_ptr<dpp::message> m = std::make_shared<dpp::message>(std::get<dpp::message>(cb2.value));
                        paginated_messages[m->id] = pages_t(player_manager->cluster, m, embeds);
                        printf("SET ID: %ld\nLEN: %ld\n", paginated_messages.find(m->id)->second.message->id, paginated_messages.size());
                    });
                }
            });
        }
    }
}
