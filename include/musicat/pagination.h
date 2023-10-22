#ifndef MUSICAT_PAGINATE_H
#define MUSICAT_PAGINATE_H

#include "musicat/player.h"
#include <any>
#include <memory>
#include <string>

namespace musicat
{
namespace paginate
{
struct pages_t
{
    dpp::cluster *client;
    std::shared_ptr<dpp::message> message;
    std::vector<dpp::embed> pages;
    size_t current;
    bool has_storage_data;

    std::mutex s_mutex;

    pages_t ();
    pages_t (dpp::cluster *client, std::shared_ptr<dpp::message> message,
             std::vector<dpp::embed> pages = {}, size_t current = 0,
             bool has_storage_data = false);
    ~pages_t ();

    /**
     * @brief Edit callback, call inside callback lambda
     *
     * @param cb
     * @param new_current
     */
    void edit_cb (const dpp::confirmation_callback_t &cb, size_t new_current);

    /**
     * @brief Edit message to page c
     *
     * @param c
     */
    void edit (size_t c, const dpp::interaction_create_t &event);

    void next (const dpp::interaction_create_t &event);
    void previous (const dpp::interaction_create_t &event);
    void home (const dpp::interaction_create_t &event);
};

extern std::map<dpp::snowflake, pages_t> paginated_messages; // EXTERN_VARIABLE

void update_page (dpp::snowflake msg_id, std::string param,
                  const dpp::interaction_create_t &event);
void delete_page (dpp::snowflake msg_id);

dpp::command_completion_event_t
get_inter_reply_cb (const dpp::interaction_create_t &event, bool paginate,
                    dpp::cluster *client, std::vector<dpp::embed> embeds,
                    std::any storage_data = std::any ());
void add_pagination_buttons (dpp::message *msg);

void handle_on_message_delete (const dpp::message_delete_t &event);
void handle_on_message_delete_bulk (const dpp::message_delete_bulk_t &event);
void gc (bool clear);

/**
 * @brief Construct and replay interaction with paginated message
 *
 * @param event
 * @param queue
 * @param edit_response edit
 */
void reply_paginated_playlist (const dpp::interaction_create_t &event,
                               std::deque<player::MCTrack> queue,
                               const std::string &title = "Queue",
                               const bool edit_response = false);
}
}

#endif // MUSICAT_PAGINATE_H
