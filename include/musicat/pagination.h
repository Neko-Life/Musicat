#ifndef MUSICAT_PAGINATE_H
#define MUSICAT_PAGINATE_H

namespace musicat {
    namespace paginate {
        using string = std::string;

        struct pages_t {
            dpp::cluster* client;
            std::shared_ptr<dpp::message> message;
            std::vector<dpp::embed> pages;
            size_t current;

            pages_t();
            pages_t(dpp::cluster* client, std::shared_ptr<dpp::message> message, std::vector<dpp::embed> pages = {}, size_t current = 0);
            ~pages_t();

            /**
             * @brief Edit callback, call inside callback lambda
             *
             * @param cb
             * @param new_current
             */
            void edit_cb(const dpp::confirmation_callback_t& cb, size_t new_current);

            /**
             * @brief Edit message to page c
             *
             * @param c
             */
            void edit(size_t c);

            void next();
            void previous();
            void home();
        };

        extern std::map<dpp::snowflake, pages_t> paginated_messages;

        void update_page(dpp::snowflake msg_id, string param);//, dpp::message* msg = NULL);
        void delete_page(dpp::snowflake msg_id);

        dpp::command_completion_event_t get_inter_reply_cb(const dpp::interaction_create_t& event, bool paginate, dpp::cluster* client, std::vector<dpp::embed> embeds);
        void add_pagination_buttons(dpp::message* msg);

        void handle_on_message_delete(const dpp::message_delete_t& event);
        void handle_on_message_delete_bulk(const dpp::message_delete_bulk_t& event);
        void gc();
    }
}

#endif // MUSICAT_PAGINATE_H
