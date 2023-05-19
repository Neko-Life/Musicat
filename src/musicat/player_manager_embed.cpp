#include "musicat/musicat.h"
#include "musicat/player.h"
#include <variant>

namespace musicat
{
namespace player
{
// these are the most ridiculous code I have ever wrote....
using string = std::string;

bool
Manager::send_info_embed (dpp::snowflake guild_id, bool update,
                          bool force_playing_status,
                          const dpp::interaction_create_t *event)
{
    auto player = this->get_player (guild_id);
    if (!player)
        throw exception ("No player");

    const bool debug = get_debug_state ();

    if (update && !player->info_message)
        {
            if (debug)
                printf ("[MANAGER:SEND_INFO_EMBED] No message to update\n");
            return false;
        }

    auto channel_id = player->channel_id;

    auto g = dpp::find_guild (guild_id);
    auto c = dpp::find_channel (channel_id);
    bool embed_perms = has_permissions (
        g, &this->cluster->me, c,
        { dpp::p_view_channel, dpp::p_send_messages, dpp::p_embed_links });
    bool view_history_perm = has_permissions (g, &this->cluster->me, c,
                                              { dpp::p_read_message_history });

    bool delete_original = false;
    if (update)
        {
            if (!view_history_perm
                || (player->info_message
                    && player->info_message->is_source_message_deleted ()))
                {
                    update = false;
                    delete_original = true;
                }
        }
    else
        {
            if (!embed_perms)
                throw exception ("No permission");
        }

    dpp::embed e;
    try
        {
            e = get_playing_info_embed (guild_id, force_playing_status);
        }
    catch (const exception &e)
        {
            fprintf (stderr,
                     "[ERROR MANAGER::SEND_INFO_EMBED] Failed to "
                     "get_playing_info_embed: %s\n",
                     e.what ());
            return false;
        }
    catch (const dpp::exception &e)
        {
            fprintf (stderr,
                     "[ERROR MANAGER::SEND_INFO_EMBED] Failed to "
                     "get_playing_info_embed [dpp::exception]: %s\n",
                     e.what ());
            return false;
        }
    catch (const std::logic_error &e)
        {
            fprintf (stderr,
                     "[ERROR MANAGER::SEND_INFO_EMBED] Failed to "
                     "get_playing_info_embed [std::logic_error]: %s\n",
                     e.what ());
            return false;
        }

    auto m_cb = [this, player, debug] (dpp::confirmation_callback_t cb) {
        if (cb.is_error ())
            {
                fprintf (stderr,
                         "[ERROR MANAGER::SEND_INFO_EMBED] message_create "
                         "callback error:\nmes: %s\ncode: %d\nerr:\n",
                         cb.get_error ().message.c_str (),
                         cb.get_error ().code);
                for (const auto &i : cb.get_error ().errors)
                    fprintf (stderr, "c: %s\nf: %s\no: %s\nr: %s\n",
                             i.code.c_str (), i.field.c_str (),
                             i.object.c_str (), i.reason.c_str ());
                return;
            }

        if (cb.value.index ())
            {
                if (!player)
                    {
                        fprintf (stderr,
                                 "[ERROR MANAGER::SEND_INFO_EMBED] PLAYER "
                                 "GONE WTFF\n");
                        return;
                    }

                if (std::holds_alternative<dpp::message> (cb.value))
                    {
                        std::lock_guard<std::mutex> lk (this->imc_m);

                        if (player->info_message)
                            {
                                auto id = player->info_message->id;
                                this->info_messages_cache.erase (id);
                            }

                        player->info_message = std::make_shared<dpp::message> (
                            std::get<dpp::message> (cb.value));

                        if (player->info_message)
                            {
                                this->info_messages_cache[player->info_message
                                                              ->id]
                                    = player->info_message;
                                if (debug)
                                    printf ("[MANAGER::SEND_INFO_EMBED] New "
                                            "message info: %ld\n",
                                            player->info_message->id);
                            }
                    }
            }
        else if (debug)
            printf ("[MANAGER::SEND_INFO_EMBED] No message_create cb size\n");
    };

    if (delete_original)
        {
            this->delete_info_embed (guild_id);
        }

    // TODO: Refactor this horrendous `update` flag system and check for
    // existing info_message instead
    if (!update)
        {
            dpp::message m;
            m.add_embed (e).set_channel_id (channel_id);

            m.add_component (dpp::component ().add_component (
                dpp::component ()
                    .set_label ("Update")
                    .set_id ("playnow/u")
                    .set_type (dpp::cot_button)
                    .set_style (dpp::cos_primary)));

            if (event)
                event->reply (m, m_cb);
            else
                this->cluster->message_create (m, m_cb);
        }
    else if (player->info_message)
        {
            auto mn = *player->info_message;
            if (!mn.embeds.empty ())
                mn.embeds.pop_back ();
            mn.embeds.push_back (e);

            if (debug)
                printf ("[MANAGER::SEND_INFO_EMBED] Channel Info Embed Id "
                        "Edit: %ld %ld\n",
                        mn.channel_id, mn.id);

            if (event)
                {
                    dpp::interaction_response reply (dpp::ir_update_message,
                                                     mn);
                    event->from->creator->interaction_response_create (
                        event->command.id, event->command.token, reply, m_cb);
                }
            else
                this->cluster->message_edit (mn, m_cb);
        }
    return true;
}

bool
Manager::update_info_embed (dpp::snowflake guild_id, bool force_playing_status, const dpp::interaction_create_t *event)
{
    if (get_debug_state ())
        printf ("[MANAGER::UPDATE_INFO_EMBED] Update info called\n");
    return this->send_info_embed (guild_id, true, force_playing_status, event);
}

bool
Manager::delete_info_embed (dpp::snowflake guild_id,
                            dpp::command_completion_event_t callback)
{
    auto player = this->get_player (guild_id);
    if (!player)
        return false;

    auto retdel = [player, this] () {
        std::lock_guard<std::mutex> lk (this->imc_m);
        if (player->info_message)
            {
                auto id = player->info_message->id;
                this->info_messages_cache.erase (id);
                return true;
            }
        else
            return false;
    };

    if (!player->info_message)
        return false;
    if (player->info_message->is_source_message_deleted ())
        return retdel ();

    auto mid = player->info_message->id;
    auto cid = player->info_message->channel_id;

    if (get_debug_state ())
        printf (
            "[MANAGER::UPDATE_INFO_EMBED] Channel Info Embed Id Delete: %ld\n",
            cid);
    this->cluster->message_delete (mid, cid, callback);
    return retdel ();
}

dpp::embed
Manager::get_playing_info_embed (dpp::snowflake guild_id,
                                 bool force_playing_status)
{
    auto guild_player = this->get_player (guild_id);
    if (!guild_player)
        throw exception ("No player");

    // const bool debug = get_debug_state();
    /* if (debug) printf("[Manager::get_playing_info_embed] Locked player::t_mutex: %ld\n", guild_player->guild_id); */
    /* std::lock_guard<std::mutex> lk (guild_player->t_mutex); */

    // Reset shifted tracks
    guild_player->reset_shifted ();
    MCTrack track;
    MCTrack prev_track;
    MCTrack next_track;
    MCTrack skip_track;

    {
        auto siz = guild_player->queue.size ();
        if (!siz)
            {
                /* if (debug) printf("[Manager::get_playing_info_embed] Should unlock player::t_mutex: %ld\n", guild_player->guild_id); */
                throw exception ("No track");
            }

        if (util::player_has_current_track (guild_player))
            track = guild_player->current_track;

        else track = guild_player->queue.front ();

        prev_track = guild_player->queue.at (siz - 1UL);

        auto lm = guild_player->loop_mode;
        if (lm == loop_mode_t::l_queue)
            next_track = (siz == 1UL) ? track : guild_player->queue.at (1);
        else if (lm == loop_mode_t::l_none)
            {
                if (siz > 1UL)
                    next_track = guild_player->queue.at (1);
            }
        else
            {
                // if (loop mode == one | one/queue)
                next_track = track;
                if (siz > 1UL)
                    skip_track = guild_player->queue.at (1);
            }
    }

    dpp::guild_member o
        = dpp::find_guild_member (guild_id, this->cluster->me.id);
    dpp::guild_member u = dpp::find_guild_member (guild_id, track.user_id);
    dpp::user *uc = dpp::find_user (u.user_id);
    uint32_t color = 0;
    for (auto i : o.roles)
        {
            auto r = dpp::find_role (i);
            if (!r || !r->colour)
                continue;
            color = r->colour;
            break;
        }

    string eaname = u.nickname.length () ? u.nickname : uc->username;
    dpp::embed_author ea;
    ea.name = eaname;
    auto ua = u.get_avatar_url (4096);
    auto uca = uc->get_avatar_url (4096);
    if (ua.length ())
        ea.icon_url = ua;
    else if (uca.length ())
        ea.icon_url = uca;

    static const char *l_mode[]
        = { "Repeat one", "Repeat queue", "Repeat one/queue" };
    static const char *p_mode[] = { "Paused", "Playing" };

    string et = track.bestThumbnail ().url;
    dpp::embed e;
    e.set_description (track.snippetText ())
        .set_title (track.title ())
        .set_url (track.url ())
        .set_author (ea);

    if (!prev_track.raw.is_null ())
        e.add_field (
            "PREVIOUS",
            "[" + prev_track.title () + "](" + prev_track.url () + ")", true);
    if (!next_track.raw.is_null ())
        e.add_field (
            "NEXT", "[" + next_track.title () + "](" + next_track.url () + ")",
            true);
    if (!skip_track.raw.is_null ())
        e.add_field ("SKIP", "[" + skip_track.title () + "]("
                                 + skip_track.url () + ")");

    string ft = "";

    bool tinfo = !track.info.raw.is_null ();
    if (tinfo)
        {
            track_progress prog = util::get_track_progress(track);
            ft += "[" + format_duration (prog.current_ms) + "/" + format_duration (prog.duration) + "]";
        }

    bool has_p = false;

    if (guild_player->from)
        {
            auto con = guild_player->from->get_voice (guild_id);
            if (con && con->voiceclient)
                if (con->voiceclient->get_secs_remaining () > 0.05f)
                    {
                        has_p = true;
                        if (ft.length ())
                            ft += " | ";
                        if (con->voiceclient->is_paused ())
                            ft += p_mode[0];
                        else
                            ft += p_mode[1];
                    }
        }
    if (force_playing_status && !has_p)
        {
            if (ft.length ())
                ft += " | ";
            ft += p_mode[1];
        }

    if (guild_player->loop_mode)
        {
            if (ft.length ())
                ft += " | ";
            ft += l_mode[guild_player->loop_mode - 1];
        }
    if (guild_player->auto_play)
        {
            if (ft.length ())
                ft += " | ";
            ft += "Autoplay";
            if (guild_player->max_history_size)
                ft += string (" (")
                      + std::to_string (guild_player->max_history_size) + ")";
        }
    if (tinfo)
        {
            if (ft.length ())
                ft += " | ";
            ft += string ("[") + std::to_string (track.info.average_bitrate ())
                  + "]";
        }
    if (ft.length ())
        e.set_footer (ft, "");
    if (color)
        e.set_color (color);
    if (et.length ())
        e.set_image (et);

    /* if (debug) printf("[Manager::get_playing_info_embed] Should unlock player::t_mutex: %ld\n", guild_player->guild_id); */
    return e;
}

} // player
} // musicat
