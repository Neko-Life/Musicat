#include "musicat/pagination.h"
#include "musicat/function_macros.h"
#include "musicat/mctrack.h"
#include "musicat/musicat.h"
#include "musicat/player.h"
#include "musicat/storage.h"
#include "musicat/util.h"
#include "musicat/util_response.h"
#include <dpp/dpp.h>
#include <map>
#include <memory>
#include <stdio.h>
#include <string>
#include <variant>
#include <vector>

namespace musicat
{
namespace paginate
{
std::map<dpp::snowflake, pages_t> paginated_messages = {}; // EXTERN_VARIABLE

void
delete_page (dpp::snowflake msg_id)
{
    auto del = paginated_messages.find (msg_id);
    if (del != paginated_messages.end ())
        {
            if (del->second.has_storage_data)
                {
                    storage::remove (msg_id);
                }

            paginated_messages.erase (del);
        }
}

pages_t::pages_t ()
{
    this->client = NULL;
    this->message = {};
    this->pages = {};
    this->current = 0;
    this->has_storage_data = false;
}

pages_t::pages_t (dpp::cluster *client, std::shared_ptr<dpp::message> message,
                  std::vector<dpp::embed> pages, size_t current,
                  bool has_storage_data)
{
    this->client = client;
    this->message = message;
    this->pages = pages;
    this->current = current;
    this->has_storage_data = has_storage_data;
}

pages_t::~pages_t () = default;

void
pages_t::edit_cb (const dpp::confirmation_callback_t &cb, size_t new_current)
{
    std::lock_guard lk (this->s_mutex);

    if (cb.is_error ())
        {
            fprintf (stderr,
                     "[ERROR PAGINATE EDIT_CB]:\nmes: %s\ncode: %d\nerr:\n",
                     cb.get_error ().message.c_str (), cb.get_error ().code);

            for (const auto &i : cb.get_error ().errors)
                fprintf (stderr, "c: %s\nf: %s\no: %s\nr: %s\n",
                         i.code.c_str (), i.field.c_str (), i.object.c_str (),
                         i.reason.c_str ());

            return;
        }

    if (new_current == std::string::npos)
        {
            delete_page (this->message->id);
            return;
        }

    bool debug = get_debug_state ();

    if (std::holds_alternative<dpp::message> (cb.value))
        {
            this->message = std::make_shared<dpp::message> (
                std::get<dpp::message> (cb.value));
        }
    else if (debug)
        fprintf (stderr, "[PAGES_T EDIT_CB] No edit_cb size\n");

    this->current = new_current;
}

void
pages_t::edit (size_t c, const dpp::interaction_create_t &event)
{
    std::lock_guard lk (this->s_mutex);
    const bool debug = get_debug_state ();

    if (this->current == c)
        {
            if (debug)
                fprintf (stderr, "[RETURN PAGES_T EDIT] (current == c)\n");

            event.reply ([] (const dpp::confirmation_callback_t &e) {
                if (e.is_error ())
                    {
                        util::log_confirmation_error (
                            e, "[pages_t::edit event.reply ERROR] "
                               "(current == c)");
                    }
            });

            return;
        }

    bool disable_components = false;
    {
        time_t t;
        time (&t);
        double L = this->message->get_creation_time ();
        disable_components = (t - (time_t)L) > ONE_HOUR_SECOND;
    }

    if (!has_permissions (dpp::find_guild (this->message->guild_id),
                          &this->client->me,
                          dpp::find_channel (this->message->channel_id),
                          { dpp::p_read_message_history }))
        {
            if (disable_components)
                delete_page (this->message->id);

            return;
        }

    dpp::embed to = this->pages.at (c);
    this->message->embeds.clear ();
    this->message->add_embed (to);

    if (disable_components)
        {
            if (debug)
                fprintf (stderr, "[PAGES_T EDIT] (t > 3600)\n");

            c = std::string::npos;

            for (auto &i : this->message->components)
                for (auto &a : i.components)
                    a.set_disabled (true);
        }

    dpp::interaction_response reply (dpp::ir_update_message, *this->message);

    event.from->creator->interaction_response_create (
        event.command.id, event.command.token, reply,
        [this, c] (const dpp::confirmation_callback_t &cb) {
            this->edit_cb (cb, c);
        });

    // this->client->message_edit (
    //     *this->message, [this, c] (const dpp::confirmation_callback_t &cb) {
    //         this->edit_cb (cb, c);
    //     });
}

void
pages_t::next (const dpp::interaction_create_t &event)
{
    size_t c = this->current;
    if (c == (this->pages.size () - 1))
        {
            c = 0;
        }
    else
        c++;
    this->edit (c, event);
}

void
pages_t::previous (const dpp::interaction_create_t &event)
{
    size_t c = this->current;
    if (c == 0)
        {
            c = this->pages.size () - 1;
        }
    else
        c--;
    this->edit (c, event);
}

void
pages_t::home (const dpp::interaction_create_t &event)
{
    this->edit (0, event);
}

void
update_page (const dpp::snowflake &msg_id, const std::string &param,
             const dpp::interaction_create_t &event)
{ //, dpp::message* msg)
    if (param.empty ())
        return;

    auto a = paginated_messages.find (msg_id);
    if (a == paginated_messages.end ())
        {
            // if (msg)
            // {
            //     for (auto& i : msg->components)
            //         for (auto& c : i.components)
            //             c.set_disabled(true);
            //     msg->owner->message_edit(*msg, [msg](const
            //     dpp::confirmation_callback_t& cb) {
            //         delete msg;
            //         if (cb.is_error())
            //         {
            //             fprintf(stderr, "[ERROR(queue.134)] %s\n",
            //             cb.get_error().message.c_str()); return;
            //         }
            //     });
            // }

            event.reply (
                util::response::str_mention_user (event.command.usr.id)
                + util::rand_item<std::string> (
                    { "The book for this message is missing!",
                      "Unfortunately, the pages for this message has been "
                      "burned",
                      "Sorry I can't find any information about pages for "
                      "this message",
                      "Can't", "I'm unable, to flip the page...",
                      "Try again after a rewrite", "A good day to be lazy",
                      "The page for this message _might_ be processed in 3 to "
                      "5 business day" }));

            return;
        }

    auto &b = a->second;
    if (param == "n")
        {
            b.next (event);
            return;
        }
    else if (param == "p")
        {
            b.previous (event);
            return;
        }
    else if (param == "h")
        {
            b.home (event);
            return;
        }
    else if (param == "j")
        {
            dpp::component input = util::response::create_short_text_input (
                "Page number to jump to:", "j");

            dpp::interaction_modal_response modal ("page_queue",
                                                   "Jump to page");
            modal.add_component (input);
            event.dialog (modal);
            return;
        }

    char invalid_char = util::valid_number (param);
    if (invalid_char != 0)
        {
            event.reply (
                util::response::str_mention_user (event.command.usr.id)
                + util::rand_item<std::string> (
                    { "That isn't a number!",
                      "maybe some ppl never got to school, such sad :(",
                      "wrong number, guess again!", "ur father",
                      "maybe maybe maybe",
                      "umm i have, that one thing for u to count.. what was "
                      "the name again.. OH YEAH! KALKULATOR" }));

            return;
        }

    try
        {
            int64_t pn = std::stoll (param) - 1;
            int64_t psize = (int64_t)b.pages.size ();
            int64_t max_psize = (psize - 1);

            if (pn > max_psize)
                {
                    pn = max_psize;
                }
            else if (pn < 0)
                {
                    pn = 0;
                }

            b.edit (pn, event);
            return;
        }
    catch (const std::out_of_range &e)
        {
            event.reply (
                util::response::str_mention_user (event.command.usr.id)
                + util::rand_item<std::string> (
                    { "no", "don't be ridiculous", "smfh", "NO wtf", "don't",
                      "you have no power here", "stop!!!", "No Way",
                      "`FATAL ERROR` You have found the "
                      "`!!!INTEGER_OVERFLOW!!!` vulnerability! "
                      "Congratulations! Here's ur reward 🍰! I'm now gonna "
                      "crash thanks to u 💔 💔" }));
        }
}

dpp::command_completion_event_t
get_inter_reply_cb (const dpp::interaction_create_t &event, bool paginate,
                    dpp::cluster *client, std::vector<dpp::embed> embeds,
                    std::any storage_data)
{
    return [event, paginate, client, embeds,
            storage_data] (const dpp::confirmation_callback_t &cb) {
        if (cb.is_error ())
            {
                fprintf (stderr, "[ERROR PAGINATE GET_INTER_REPLY_CB]: %s\n",
                         cb.get_error ().message.c_str ());

                return;
            }
        bool has_v = storage_data.has_value ();
        if (paginate || has_v)
            {
                event.get_original_response ([client, embeds, has_v,
                                              storage_data] (
                                                 const dpp::
                                                     confirmation_callback_t
                                                         &cb2) {
                    if (cb2.is_error ())
                        {
                            fprintf (
                                stderr,
                                "[ERROR PAGINATE GET_INTER_REPLY_CB]: %s\n",
                                cb2.get_error ().message.c_str ());
                            return;
                        }

                    std::shared_ptr<dpp::message> m
                        = std::make_shared<dpp::message> (
                            std::get<dpp::message> (cb2.value));

                    paginated_messages[m->id].client = client;
                    paginated_messages[m->id].message = m;
                    paginated_messages[m->id].pages = embeds;
                    paginated_messages[m->id].current = 0;
                    paginated_messages[m->id].has_storage_data = has_v;

                    const bool debug = get_debug_state ();
                    if (debug)
                        std::cerr
                            << "[PAGINATE GET_INTER_REPLY_CB] PAGINATED ID: "
                            << m->id << "\nLEN: " << paginated_messages.size ()
                            << '\n';

                    if (has_v)
                        {
                            storage::set (m->id, storage_data);
                            if (debug)
                                std::cerr << "[PAGINATE GET_INTER_REPLY_CB] "
                                             "STORAGE ID: "
                                          << m->id
                                          << "\nLEN: " << storage::size ()
                                          << '\n';
                        }
                });
            }
    };
}

void
add_pagination_buttons (dpp::message *msg)
{
    msg->add_component (
        dpp::component ()
            .add_component (dpp::component ()
                                .set_emoji (MUSICAT_U8 ("◀️"))
                                .set_id ("page_queue/p")
                                .set_type (dpp::cot_button)
                                .set_style (dpp::cos_primary))
            .add_component (dpp::component ()
                                .set_emoji (MUSICAT_U8 ("🏠"))
                                .set_id ("page_queue/h")
                                .set_type (dpp::cot_button)
                                .set_style (dpp::cos_primary))
            .add_component (dpp::component ()
                                .set_emoji (MUSICAT_U8 ("🦘"))
                                .set_id ("page_queue/j")
                                .set_type (dpp::cot_button)
                                .set_style (dpp::cos_primary))
            .add_component (dpp::component ()
                                .set_emoji (MUSICAT_U8 ("▶️"))
                                .set_id ("page_queue/n")
                                .set_type (dpp::cot_button)
                                .set_style (dpp::cos_primary)));
}

void
handle_on_message_delete (const dpp::message_delete_t &event)
{
    delete_page (event.id);
}

void
handle_on_message_delete_bulk (const dpp::message_delete_bulk_t &event)
{
    for (auto i : event.deleted)
        delete_page (i);
}

void
gc (bool clear)
{
    time_t t;
    time (&t);

    size_t pg_s = paginated_messages.size ();
    const bool debug = get_debug_state ();

    if (debug)
        fprintf (stderr, "[PAGINATE_GC] SIZE BEFORE: %ld, %ld\n", pg_s,
                 storage::size ());

    size_t d = 0;
    auto i = paginated_messages.begin ();
    while (i != paginated_messages.end ())
        {
            double L = i->second.message->get_creation_time ();
            if (clear || (t - (time_t)L) > ONE_HOUR_SECOND)
                {
                    if (debug)
                        std::cerr << "Deleting " << ++d << ": "
                                  << i->second.message->id << '\n';

                    storage::remove (i->first);
                    i = paginated_messages.erase (i);
                }
            else
                i++;
        }

    if (debug)
        fprintf (stderr, "[PAGINATE_GC] SIZE AFTER: %ld, %ld\n",
                 paginated_messages.size (), storage::size ());
}

void
_construct_desc (std::deque<player::MCTrack> &queue,
                 std::deque<player::MCTrack>::iterator i, std::string &desc,
                 size_t &id, size_t &count, size_t &qs, dpp::embed &embed,
                 const std::string &title, std::vector<dpp::embed> &embeds,
                 uint64_t &totald,
                 std::shared_ptr<player::Player> guild_player)
{
    if (i == queue.begin ())
        {
            player::track_progress prog = { 0, 0, -1 };
            if (util::player_has_current_track (guild_player))
                prog = util::get_track_progress (guild_player->current_track);
            else
                prog = util::get_track_progress (*i);

            desc += "Current track: [" + mctrack::get_title (*i) + "]("
                    + mctrack::get_url (*i) + ")"
                    + std::string (
                        !prog.status
                            ? std::string (" [")
                                  + format_duration (prog.current_ms) + "/"
                                  + format_duration (prog.duration) + "]"
                            : "")
                    + " - <@" + std::to_string (i->user_id) + ">\n\n";
        }
    else
        {
            uint64_t dur = mctrack::get_duration (*i);

            desc += std::to_string (id) + ": [" + mctrack::get_title (*i)
                    + "](" + mctrack::get_url (*i) + ")"
                    + std::string (dur ? std::string (" [")
                                             + format_duration (dur) + "]"
                                       : "")
                    + " - <@" + std::to_string (i->user_id) + ">\n";

            id++;
            count++;
        }

    if ((count && !(count % 10)) || id == qs)
        {
            embed.set_title (title).set_description (
                desc.length () > 2048
                    ? "Description too long, pagination is on the way!"
                    : desc);

            std::string fot = "";

            if (totald)
                fot += format_duration (totald);

            if (qs)
                {
                    if (!fot.empty ())
                        fot += " | ";

                    fot += std::to_string (qs) + " track"
                           + (qs > 1 ? "s" : "");
                }

            if (!fot.empty ())
                embed.set_footer (fot, "");

            embeds.emplace_back (embed);
            embed = dpp::embed ();
            desc = "";
            count = 0;
        }
}

void
reply_paginated_playlist (const dpp::interaction_create_t &event,
                          std::deque<player::MCTrack> queue,
                          const std::string &title, const bool edit_response)
{
    // unused var
    // const bool debug = get_debug_state ();

    std::vector<dpp::embed> embeds = {};

    dpp::embed embed;
    // std::vector<std::string> queue_str;
    std::string desc = "";
    size_t id = 1;
    size_t qs = queue.size ();
    size_t count = 0;

    uint64_t totald = 0;

    for (auto i = queue.begin (); i != queue.end (); i++)
        totald += mctrack::get_duration (*i);

    auto guild_player
        = get_player_manager_ptr ()->get_player (event.command.guild_id);

    for (auto i = queue.begin (); i != queue.end (); i++)
        {
            _construct_desc (queue, i, desc, id, count, qs, embed, title,
                             embeds, totald, guild_player);
        }

    dpp::message msg;
    msg.add_embed (embeds.front ());

    bool paginate = embeds.size () > 1;
    if (paginate)
        {
            paginate::add_pagination_buttons (&msg);
        }

    if (!edit_response)
        event.reply (msg, paginate::get_inter_reply_cb (
                              event, paginate, event.from->creator, embeds));
    else
        event.edit_response (
            msg, paginate::get_inter_reply_cb (event, paginate,
                                               event.from->creator, embeds));

} // reply_paginated_playlist
} // paginate
} // musicat
