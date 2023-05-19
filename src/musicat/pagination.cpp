#include "musicat/musicat.h"
#include "musicat/pagination.h"
#include "musicat/player.h"
#include "musicat/storage.h"
#include <dpp/dpp.h>
#include <map>
#include <stdio.h>
#include <string>
#include <variant>
#include <vector>

#define ONE_HOUR_SECOND 3600

namespace musicat
{
namespace paginate
{
std::map<dpp::snowflake, pages_t> paginated_messages = {}; // EXTERN VARIABLE

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
    std::lock_guard<std::mutex> lk(this->s_mutex);
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
    if (cb.value.index () && std::holds_alternative<dpp::message> (cb.value))
        {
            this->message = std::make_shared<dpp::message> (
                std::get<dpp::message> (cb.value));
        }
    else if (get_debug_state ())
        printf ("[PAGES_T EDIT_CB] No edit_cb size\n");

    this->current = new_current;
}

void
pages_t::edit (size_t c, const dpp::interaction_create_t &event)
{
    std::lock_guard<std::mutex> lk(this->s_mutex);
    const bool debug = get_debug_state ();

    if (this->current == c)
        {
            if (debug)
                printf ("[RETURN PAGES_T EDIT] (current == c)\n");
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
                printf ("[PAGES_T EDIT] (t > 3600)\n");
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
    if (this->current == 0)
        return;
    this->edit (0, event);
}

void
update_page (dpp::snowflake msg_id, std::string param,
             const dpp::interaction_create_t &event)
{ //, dpp::message* msg)
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
            return;
        }
    auto &b = a->second;
    if (param == "n")
        {
            b.next (event);
        }
    else if (param == "p")
        {
            b.previous (event);
        }
    else if (param == "h")
        {
            b.home (event);
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
                        printf ("[PAGINATE GET_INTER_REPLY_CB] PAGINATED ID: "
                                "%ld\nLEN: %ld\n",
                                m->id, paginated_messages.size ());
                    if (has_v)
                        {
                            storage::set (m->id, storage_data);
                            if (debug)
                                printf ("[PAGINATE GET_INTER_REPLY_CB] "
                                        "STORAGE ID: %ld\nLEN: %ld\n",
                                        m->id, storage::size ());
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
                                .set_emoji (u8"◀️")
                                .set_id ("page_queue/p")
                                .set_type (dpp::cot_button)
                                .set_style (dpp::cos_primary))
            .add_component (dpp::component ()
                                .set_emoji (u8"🏠")
                                .set_id ("page_queue/h")
                                .set_type (dpp::cot_button)
                                .set_style (dpp::cos_primary))
            .add_component (dpp::component ()
                                .set_emoji (u8"▶️")
                                .set_id ("page_queue/n")
                                .set_type (dpp::cot_button)
                                .set_style (dpp::cos_primary)));
}

void
handle_on_message_delete (const dpp::message_delete_t &event)
{
    delete_page (event.deleted->id);
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
        printf ("[PAGINATE_GC] SIZE BEFORE: %ld, %ld\n", pg_s,
                storage::size ());
    size_t d = 0;
    auto i = paginated_messages.begin ();
    while (i != paginated_messages.end ())
        {
            double L = i->second.message->get_creation_time ();
            if (clear || (t - (time_t)L) > ONE_HOUR_SECOND)
                {
                    if (debug)
                        printf ("Deleting %ld: %ld\n", ++d,
                                i->second.message->id);
                    storage::remove (i->first);
                    i = paginated_messages.erase (i);
                }
            else
                i++;
        }
    if (debug)
        printf ("[PAGINATE_GC] SIZE AFTER: %ld, %ld\n",
                paginated_messages.size (), storage::size ());
}

void
_construct_desc (std::deque<player::MCTrack> &queue,
                 std::deque<player::MCTrack>::iterator i, std::string &desc,
                 size_t &id, size_t &count, size_t &qs, dpp::embed &embed,
                 const std::string &title, std::vector<dpp::embed> &embeds,
                 uint64_t &totald, std::shared_ptr<player::Player> guild_player)
{
    if (i == queue.begin ())
        {
            player::track_progress prog = { 0, 0, -1 };
            if (util::player_has_current_track(guild_player) && !guild_player->current_track.info.raw.is_null ())
                prog = util::get_track_progress (guild_player->current_track);
            else if (!i->info.raw.is_null ())
                prog = util::get_track_progress (*i);

            desc += "Current track: [" + i->title () + "](" + i->url () + ")"
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
            uint64_t dur = 0;
            if (!i->info.raw.is_null ())
                dur = i->info.duration ();

            desc += std::to_string (id) + ": [" + i->title () + "]("
                    + i->url () + ")"
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
                    if (fot.length ())
                        fot += " | ";
                    fot += std::to_string (qs) + " track"
                           + (qs > 1 ? "s" : "");
                }
            if (fot.length ())
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
        if (!i->info.raw.is_null ())
            totald += i->info.duration ();

    auto guild_player = get_player_manager_ptr ()->get_player (event.command.guild_id);

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
