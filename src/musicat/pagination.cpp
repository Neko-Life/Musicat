#include <map>
#include <vector>
#include <stdio.h>
#include <dpp/dpp.h>
#include "musicat/musicat.h"
#include "musicat/pagination.h"

#define ONE_HOUR_SECOND 3600

namespace musicat {
    namespace paginate {
        std::map<dpp::snowflake, pages_t> paginated_messages = {}; // EXTERN VARIABLE

        void delete_page(dpp::snowflake msg_id) {
            paginated_messages.erase(msg_id);
        }

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
                fprintf(stderr, "[ERROR(queue.24)]:\nmes: %s\ncode: %d\nerr:\n", cb.get_error().message.c_str(), cb.get_error().code);
                for (const auto& i : cb.get_error().errors)
                    fprintf(stderr, "c: %s\nf: %s\no: %s\nr: %s\n",
                        i.code.c_str(),
                        i.field.c_str(),
                        i.object.c_str(),
                        i.reason.c_str()
                    );
                return;
            }
            if (new_current == string::npos)
            {
                delete_page(this->message->id);
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
                printf("[RETURN(queue.45)] (current == c)\n");
                return;
            }
            bool disable_components = false;
            {
                time_t t;
                time(&t);
                double L = this->message->get_creation_time();
                disable_components = (t - (time_t)L) > ONE_HOUR_SECOND;
            }
            if (!musicat::has_permissions(
                dpp::find_guild(this->message->guild_id),
                &this->client->me,
                dpp::find_channel(this->message->channel_id),
                { dpp::p_read_message_history }
            ))
            {
                if (disable_components) delete_page(this->message->id);
                return;
            }

            dpp::embed to = this->pages.at(c);
            this->message->embeds.clear();
            this->message->add_embed(to);

            if (disable_components)
            {
                printf("(t > 3600)\n");
                c = string::npos;
                for (auto& i : this->message->components)
                    for (auto& a : i.components)
                        a.set_disabled(true);
            }

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

        void update_page(dpp::snowflake msg_id, string param) {//, dpp::message* msg) {
            auto a = paginated_messages.find(msg_id);
            if (a == paginated_messages.end())
            {
                // if (msg)
                // {
                //     for (auto& i : msg->components)
                //         for (auto& c : i.components)
                //             c.set_disabled(true);
                //     msg->owner->message_edit(*msg, [msg](const dpp::confirmation_callback_t& cb) {
                //         delete msg;
                //         if (cb.is_error())
                //         {
                //             fprintf(stderr, "[ERROR(queue.134)] %s\n", cb.get_error().message.c_str());
                //             return;
                //         }
                //     });
                // }
                return;
            }
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

        dpp::command_completion_event_t get_inter_reply_cb(const dpp::interaction_create_t& event, bool paginate, dpp::cluster* client, std::vector<dpp::embed> embeds) {
            return [event, paginate, client, embeds](const dpp::confirmation_callback_t& cb) {
                if (cb.is_error())
                {
                    fprintf(stderr, "[ERROR(pagination.166)]: %s\n", cb.get_error().message.c_str());
                    return;
                }
                if (paginate)
                {
                    event.get_original_response([client, embeds](const dpp::confirmation_callback_t& cb2) {
                        if (cb2.is_error())
                        {
                            fprintf(stderr, "[ERROR(pagination.174)]: %s\n", cb2.get_error().message.c_str());
                            return;
                        }
                        std::shared_ptr<dpp::message> m = std::make_shared<dpp::message>(std::get<dpp::message>(cb2.value));
                        paginated_messages[m->id] = pages_t(client, m, embeds);
                        printf("SET ID: %ld\nLEN: %ld\n", paginated_messages.find(m->id)->second.message->id, paginated_messages.size());
                    });
                }
            };
        }

        void add_pagination_buttons(dpp::message* msg) {
            msg->add_component(
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

        void handle_on_message_delete(const dpp::message_delete_t& event) {
            delete_page(event.deleted->id);
        }

        void handle_on_message_delete_bulk(const dpp::message_delete_bulk_t& event) {
            for (auto i : event.deleted) delete_page(i);
        }

        void gc() {
            time_t t;
            time(&t);

            printf("[PAGINATION_GC] SIZ0: %ld\n", paginated_messages.size());
            size_t d = 0;
            auto i = paginated_messages.begin();
            while (i != paginated_messages.end())
            {
                double L = i->second.message->get_creation_time();
                if ((t - (time_t)L) > ONE_HOUR_SECOND)
                {
                    paginated_messages.erase(i->first);
                    printf("Deleted %ld\n", ++d);
                }
                else i++;
            }
            printf("[PAGINATION_GC] SIZ1: %ld\n", paginated_messages.size());
        }
    }
}
