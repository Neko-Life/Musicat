#include "musicat/mctrack.h"
#include "musicat/musicat.h"
#include "musicat/player.h"
#include "musicat/util.h"
#include <variant>

namespace musicat
{
namespace player
{
// these are the most ridiculous code I have ever wrote....
using string = std::string;

static std::mutex pe_m;
static std::vector<dpp::snowflake> processing_embed = {};

auto
get_processing_embed (const dpp::snowflake &guild_id)
{
    std::lock_guard<std::mutex> lk (pe_m);
    return vector_find (&processing_embed, guild_id);
}

bool
is_processing_embed (const dpp::snowflake &guild_id)
{
    return get_processing_embed (guild_id) != processing_embed.end ();
}

void
set_processing_embed (const dpp::snowflake &guild_id)
{
    if (is_processing_embed (guild_id))
        return;

    std::lock_guard<std::mutex> lk (pe_m);
    processing_embed.push_back (guild_id);
}

void
clear_processing_embed (const dpp::snowflake &guild_id)
{
    auto i = get_processing_embed (guild_id);

    std::lock_guard<std::mutex> lk (pe_m);
    while (i != processing_embed.end ())
        {
            if (*i != guild_id)
                {
                    i++;
                    continue;
                }

            i = processing_embed.erase (i);
        }
}

class ProcessingEmbedClearer
{
    const dpp::snowflake guild_id;
    bool no_clear;

  public:
    ProcessingEmbedClearer (const dpp::snowflake &guild_id,
                            bool no_set_blocker = false)
        : guild_id (guild_id), no_clear (false)
    {
        if (no_set_blocker)
            return;

        set_processing_embed (this->guild_id);
    };

    void
    set_no_clear (bool no_clear)
    {
        this->no_clear = no_clear;
    }

    ~ProcessingEmbedClearer ()
    {
        if (this->no_clear)
            return;

        clear_processing_embed (this->guild_id);
    };
};

bool
Manager::send_info_embed (const dpp::snowflake &guild_id, bool update,
                          const bool force_playing_status,
                          const dpp::interaction_create_t *event)
{
    if (is_processing_embed (guild_id))
        {
            return false;
        }

    ProcessingEmbedClearer pec (guild_id);

    auto player = this->get_player (guild_id);
    if (!player)
        {
            throw exception ("No player");
        }

    const bool debug = get_debug_state ();

    if (update && !player->info_message)
        {
            if (debug)
                fprintf (stderr,
                         "[MANAGER:SEND_INFO_EMBED] No message to update\n");

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
    else if (!embed_perms)
        {
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

    auto m_cb = [this, guild_id] (dpp::confirmation_callback_t cb) {
        ProcessingEmbedClearer pec (guild_id, true);

        if (cb.is_error ())
            {
                auto e_i = cb.get_error ();

                fprintf (stderr,
                         "[Manager::send_info_embed ERROR] "
                         "message_create callback error:\n"
                         "mes: %s\ncode: %d\nerr:\n",
                         e_i.message.c_str (), e_i.code);

                for (const auto &i : e_i.errors)
                    fprintf (stderr, "c: %s\nf: %s\no: %s\nr: %s\n",
                             i.code.c_str (), i.field.c_str (),
                             i.object.c_str (), i.reason.c_str ());

                return;
            }

        bool debug = get_debug_state ();

        if (!std::holds_alternative<dpp::message> (cb.value))
            {
                if (debug)
                    fprintf (stderr, "[Manager::send_info_embed] "
                                     "No message_create cb size\n");

                return;
            }

        auto player = this->get_player (guild_id);

        if (!player)
            {
                fprintf (stderr, "[Manager::send_info_embed ERROR] PLAYER "
                                 "GONE WTFF\n");

                return;
            }

        std::lock_guard<std::mutex> lk (this->imc_m);

        dpp::snowflake id;

        if (player->info_message)
            {
                id = player->info_message->id;
                this->info_messages_cache.erase (id);
            }

        player->info_message = std::make_shared<dpp::message> (
            std::get<dpp::message> (cb.value));

        if (!player->info_message)
            return;

        id = player->info_message->id;

        this->info_messages_cache[id] = player->info_message;

        if (debug)
            std::cerr << "[MANAGER::SEND_INFO_EMBED] New "
                         "message info: "
                      << id << '\n';
    };

    if (delete_original)
        {
            // no return? sending delete while also edit under it?
            //
            // no. update will always be false when delete_original is true
            // or at least it should

            this->delete_info_embed (guild_id);
        }

    // TODO: Refactor this horrendous `update` flag system and check for
    // existing info_message instead

    // not update
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

            // m_cb is used after here, set no_clear to clear it on callback
            pec.set_no_clear (true);

            if (event && !delete_original)
                {
                    event->reply (m, m_cb);
                    return true;
                }

            this->cluster->message_create (m, m_cb);
            return true;
        }

    // else update

    if (!player->info_message)
        // !TODO: shouldn't be false? is it safe to return false?
        return true;

    auto mn = *player->info_message;
    if (!mn.embeds.empty ())
        mn.embeds.pop_back ();

    mn.embeds.push_back (e);

    if (debug)
        std::cerr << "[MANAGER::SEND_INFO_EMBED] Channel Info Embed Id "
                     "Edit: "
                  << mn.channel_id << " " << mn.id << '\n';

    // m_cb is used after here, set no_clear to clear it on callback
    pec.set_no_clear (true);

    if (!event)
        {
            this->cluster->message_edit (mn, m_cb);
            return true;
        }

    dpp::interaction_response reply (dpp::ir_update_message, mn);
    event->from->creator->interaction_response_create (
        event->command.id, event->command.token, reply, m_cb);

    return true;
}

bool
Manager::update_info_embed (const dpp::snowflake &guild_id,
                            const bool force_playing_status,
                            const dpp::interaction_create_t *event)
{
    if (get_debug_state ())
        fprintf (stderr, "[MANAGER::UPDATE_INFO_EMBED] Update info called\n");

    return this->send_info_embed (guild_id, true, force_playing_status, event);
}

static bool
delete_info_embed_retdel (Manager *m, std::shared_ptr<Player> player)
{
    std::lock_guard<std::mutex> lk (m->imc_m);

    if (player->info_message)
        {
            auto id = player->info_message->id;
            m->info_messages_cache.erase (id);
            return true;
        }

    return false;
}

bool
Manager::delete_info_embed (const dpp::snowflake &guild_id,
                            dpp::command_completion_event_t callback)
{
    auto player = this->get_player (guild_id);
    if (!player)
        return false;

    if (!player->info_message)
        return false;

    if (player->info_message->is_source_message_deleted ())
        return delete_info_embed_retdel (this, player);

    auto mid = player->info_message->id;
    auto cid = player->info_message->channel_id;

    if (get_debug_state ())
        std::cerr
            << "[MANAGER::UPDATE_INFO_EMBED] Channel Info Embed Id Delete: "
            << cid << '\n';

    this->cluster->message_delete (
        mid, cid,
        [this, guild_id, callback] (const dpp::confirmation_callback_t &res) {
            if (!res.is_error ())
                {
                    auto player = this->get_player (guild_id);
                    if (player)
                        player->info_message = nullptr;
                }

            callback (res);
        });

    return delete_info_embed_retdel (this, player);
}

dpp::embed
Manager::get_playing_info_embed (const dpp::snowflake &guild_id,
                                 const bool force_playing_status)
{
    auto guild_player = this->get_player (guild_id);
    if (!guild_player)
        throw exception ("No player");

    // const bool debug = get_debug_state();
    /* if (debug) printf("[Manager::get_playing_info_embed] Locked
     * player::t_mutex: %ld\n", guild_player->guild_id); */
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
                /* if (debug) printf("[Manager::get_playing_info_embed] Should
                 * unlock player::t_mutex: %ld\n", guild_player->guild_id); */
                throw exception ("No track");
            }

        if (util::player_has_current_track (guild_player))
            track = guild_player->current_track;

        else
            track = guild_player->queue.front ();

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

    // one char variable name for 100x performance improvement!!!!!
    dpp::guild_member u;
    bool member_found = false;

    try
        {
            u = dpp::find_guild_member (guild_id, track.user_id);
            member_found = true;
        }
    catch (...)
        {
        }

    dpp::user *uc = dpp::find_user (track.user_id);

    auto h_r = util::get_user_highest_role (guild_id, get_sha_id ());

    uint32_t color = h_r ? h_r->colour : 0;

    string eaname;

    if (member_found)
        {
            std::string nick = u.get_nickname ();

            if (!nick.empty ())
                {
                    eaname = nick;
                }
        }

    if (!eaname.length () && uc)
        eaname = uc->username;
    else
        eaname = "`[ERROR]` User not found";

    dpp::embed_author ea;
    ea.name = eaname;

    std::string ua = member_found ? u.get_avatar_url (4096) : "";

    if (ua.empty () && uc)
        ua = uc->get_avatar_url (4096);

    if (!ua.empty ())
        ea.icon_url = ua;

    static const char *l_mode[]
        = { "Repeat one", "Repeat queue", "Repeat one/queue" };

    static const char *p_mode[] = { "Paused", "Playing" };

    string et = mctrack::get_thumbnail (track);

    dpp::embed e;
    e.set_description (mctrack::get_description (track))
        .set_title (mctrack::get_title (track))
        .set_url (mctrack::get_url (track))
        .set_author (ea);

    if (!prev_track.raw.is_null ())
        e.add_field ("PREVIOUS",
                     "[" + mctrack::get_title (prev_track) + "]("
                         + mctrack::get_url (prev_track) + ")",
                     true);

    if (!next_track.raw.is_null ())
        e.add_field ("NEXT",
                     "[" + mctrack::get_title (next_track) + "]("
                         + mctrack::get_url (next_track) + ")",
                     true);

    if (!skip_track.raw.is_null ())
        e.add_field ("SKIP", "[" + mctrack::get_title (skip_track) + "]("
                                 + mctrack::get_url (skip_track) + ")");

    string ft = "";

    bool tinfo = !track.info.raw.is_null ();
    if (tinfo)
        {
            track_progress prog = util::get_track_progress (track);
            ft += "[" + format_duration (prog.current_ms) + "/"
                  + format_duration (prog.duration) + "]";
        }

    bool has_p = false;

    if (guild_player->from)
        {
            auto con = guild_player->from->get_voice (guild_id);
            if (con && con->voiceclient
                && con->voiceclient->get_secs_remaining () > 0.05f)
                {
                    has_p = true;
                    if (!ft.empty ())
                        ft += " | ";

                    if (con->voiceclient->is_paused ())
                        ft += p_mode[0];
                    else
                        ft += p_mode[1];
                }
        }

    if (force_playing_status && !has_p)
        {
            if (!ft.empty ())
                ft += " | ";

            ft += p_mode[1];
        }

    if (guild_player->loop_mode)
        {
            if (!ft.empty ())
                ft += " | ";

            ft += l_mode[guild_player->loop_mode - 1];
        }

    if (guild_player->auto_play)
        {
            if (!ft.empty ())
                ft += " | ";

            ft += "Autoplay";

            if (guild_player->max_history_size)
                ft += string (" (")
                      + std::to_string (guild_player->max_history_size) + ")";
        }

    if (tinfo)
        {
            if (!ft.empty ())
                ft += " | ";

            ft += string ("[") + std::to_string (track.info.average_bitrate ())
                  + "]";
        }

    int64_t rpt = guild_player->current_track.repeat;
    if (rpt > 0)
        {
            if (!ft.empty ())
                ft += " | ";

            ft += string ("R ") + std::to_string (rpt);
        }

    if (!ft.empty ())
        e.set_footer (ft, "");

    if (color)
        e.set_color (color);

    if (!et.empty ())
        e.set_image (et);

    return e;
}

} // player
} // musicat
