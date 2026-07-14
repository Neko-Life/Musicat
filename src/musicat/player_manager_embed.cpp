#include "message.h"
#include "musicat/function_macros.h"
#include "musicat/mctrack.h"
#include "musicat/musicat.h"
#include "musicat/player.h"
#include "musicat/util.h"
#include "snowflake.h"
#include <mutex>
#include <variant>

namespace musicat
{
namespace player
{
// these are the most ridiculous code I have ever wrote....

// EMBED OPERATION QUEUE //////////////////////////////////////////////////////////////////////////////
struct embed_op_t
{
    bool update;
    bool force_playing_status;
    bool has_event;
    bool delete_original;
    dpp::snowflake guild_id;
    dpp::snowflake channel_id;
    dpp::interaction_create_t event;
    dpp::command_completion_event_t message_callback;
    dpp::message prev_message;
};

static bool
is_embed_op_duplicate (const embed_op_t &is_this, const embed_op_t &duplicate)
{
    return is_this.guild_id == duplicate.guild_id && is_this.channel_id == duplicate.channel_id;
}

static std::vector<embed_op_t> embed_q;
static std::mutex embed_q_m;

static void
run_embed_op (const embed_op_t &o)
{
    auto *manager = get_player_manager_ptr ();
    if (!manager)
        return;

    if (!o.update)
        {
            dpp::message m;
            if (manager->get_playing_info_message (m, o.guild_id, o.force_playing_status, false) != 0)
                return;

            m.set_channel_id (o.channel_id);

            if (o.has_event && !o.delete_original)
                {
                    o.event.reply (m, o.message_callback);
                    return;
                }

            manager->cluster->message_create (m, o.message_callback);
            return;
        }

    auto pm = o.prev_message;
    if (manager->get_playing_info_message (pm, o.guild_id, o.force_playing_status, player::playing_info_utils::is_button_expanded (pm))
        != 0)
        return;

    if (!o.has_event)
        {
            manager->cluster->message_edit (pm, o.message_callback);
            return;
        }

    dpp::interaction_response reply (dpp::ir_update_message, pm);
    o.event.from ()->creator->interaction_response_create (o.event.command.id, o.event.command.token, reply, o.message_callback);
}

static void
queue_embed_op (const embed_op_t &o)
{
    std::lock_guard lk (embed_q_m);
    auto i = embed_q.begin ();
    while (i != embed_q.end ())
        {
            if (!is_embed_op_duplicate (*i, o))
                {
                    i++;
                    continue;
                }
            i = embed_q.erase (i);
            break;
        }
    embed_q.push_back (o);
}

void
check_embed_op_queue ()
{
    std::lock_guard lk (embed_q_m);
    auto i = embed_q.begin ();
    while (i != embed_q.end ())
        {
            run_embed_op (*i);
            i = embed_q.erase (i);
        }
}

////////////////////////////////////////////////////////////////////////////////

static void
send_info_embed_cb (const dpp::confirmation_callback_t &cb, const dpp::snowflake &guild_id)
{
    if (cb.is_error ())
        {
            auto e_i = cb.get_error ();

            fprintf (stderr, "[player::send_info_embed_cb ERROR] message_create callback error:\nmes: %s\ncode: %d\nerr:\n",
                     e_i.message.c_str (), e_i.code);

            for (const auto &i : e_i.errors)
                fprintf (stderr, "c: %s\nf: %s\no: %s\nr: %s\n", i.code.c_str (), i.field.c_str (), i.object.c_str (), i.reason.c_str ());

            return;
        }

    bool debug = get_debug_state ();

    if (!std::holds_alternative<dpp::message> (cb.value))
        {
            if (debug)
                fprintf (stderr, "[player::send_info_embed_cb] No message_create cb size\n");

            return;
        }

    auto player_manager_ptr = get_player_manager_ptr ();
    if (!player_manager_ptr)
        {
            fprintf (stderr, "[player::send_info_embed_cb ERROR] Missing player manager\n");
            return;
        }

    auto player = player_manager_ptr->get_player (guild_id);
    if (!player)
        {
            fprintf (stderr, "[player::send_info_embed_cb ERROR] PLAYER GONE WTFF\n");
            return;
        }

    dpp::message new_message = std::get<dpp::message> (cb.value);
    player->set_info_message (new_message);

    if (debug)
        std::cerr << "[player::send_info_embed_cb] New message info: " << new_message.id.str () << '\n';
}

void
Manager::send_info_embed (const dpp::snowflake &guild_id, bool update, const bool force_playing_status,
                          const dpp::interaction_create_t *event)
{
    auto *manager = get_player_manager_ptr ();
    if (!manager)
        return;

    auto player = manager->get_player (guild_id);
    if (!player)
        throw exception ("No player");

    const bool debug = get_debug_state ();

    auto pim = player->get_info_message ();
    const bool cant_update = !event && pim.second != 0;

    if (update && cant_update)
        {
            if (debug)
                fprintf (stderr, "[Manager::send_info_embed] No message to update\n");

            return;
        }

    auto channel_id = player->channel_id;

    auto g = dpp::find_guild (guild_id);
    auto c = dpp::find_channel (channel_id);

    bool embed_perms = has_permissions (g, &manager->cluster->me, c, { dpp::p_view_channel, dpp::p_send_messages, dpp::p_embed_links });

    bool view_history_perm = has_permissions (g, &manager->cluster->me, c, { dpp::p_read_message_history });

    bool delete_original = false;
    if (update)
        {
            auto pim = player->get_info_message ();
            if (pim.second != 0 || pim.first.is_source_message_deleted () || !view_history_perm)
                {
                    update = false;
                    delete_original = true;
                }
        }
    else if (!embed_perms)
        throw exception ("No permission");

    auto m_cb = [guild_id] (dpp::confirmation_callback_t cb) { send_info_embed_cb (cb, guild_id); };

    if (delete_original)
        {
            // no return? sending delete while also edit under it?
            //
            // no. update will always be false when delete_original is true
            // or at least it should
            assert (update == false);

            manager->delete_info_embed (guild_id);
        }

    // TODO: Refactor this horrendous `update` flag system and check for
    // existing info_message instead

    dpp::message mn;
    bool has_msg_to_update = false;
    bool invalid_update = false;

    if (update)
        {
            if (event)
                {
                    mn = event->command.msg;

                    // if cached message has same id then update its json
                    // else keep previous cache, this event probably come from /playing cmd
                    if (pim.second == 0 && pim.first.id == mn.id)
                        {
                            player->set_info_message (mn);
                            pim.first = mn;
                        }

                    has_msg_to_update = true;
                }
            else if (pim.second == 0)
                {
                    mn = pim.first;
                    has_msg_to_update = true;
                }

            invalid_update = update && !mn.channel_id && !mn.id;
            if (invalid_update && debug)
                {
                    std::cerr << "[Manager::send_info_embed WARN] Invalid update for gid(" << guild_id << ") cid(" << channel_id
                              << "), fallback to create\n";
                }
        }

    // not update
    if (!update || invalid_update)
        {
            queue_embed_op ({ false,
                              force_playing_status,
                              event != nullptr,
                              delete_original,
                              guild_id,
                              channel_id,
                              event ? *event : dpp::interaction_create_t{},
                              m_cb,
                              {} });
            return;
        }

    // else update

    if (!has_msg_to_update)
        return;

    if (debug)
        std::cerr << "[Manager::send_info_embed] Channel Info Embed Id Edit: " << mn.channel_id << " " << mn.id << '\n';

    if (invalid_update)
        {
            if (debug)
                std::cerr << "[Manager::send_info_embed] Invalid update, canceling Info Embed Update" << '\n';

            return;
        }

    queue_embed_op ({
        true,
        force_playing_status,
        event != nullptr,
        delete_original,
        guild_id,
        channel_id,
        event ? *event : dpp::interaction_create_t{},
        m_cb,
        mn,
    });
}

void
Manager::update_info_embed (const dpp::snowflake &guild_id, const bool force_playing_status, const dpp::interaction_create_t *event)
{
    if (get_debug_state ())
        fprintf (stderr, "[Manager::update_info_embed] Update info called\n");

    this->send_info_embed (guild_id, true, force_playing_status, event);
}

void
Manager::reply_info_embed (const dpp::interaction_create_t &event, bool expand_button, bool reply_update_message)
{
    dpp::message m;

    if (this->get_playing_info_message (m, event.command.guild_id, false, expand_button) != 0)
        {
            return event.reply ("`[ERROR]` Something went wrong when getting playback info");
        }

    if (reply_update_message)
        {
            dpp::interaction_response reply (dpp::ir_update_message, m);
            event.from ()->creator->interaction_response_create (event.command.id, event.command.token, reply);
        }
    else
        event.reply (m);
}

bool
Manager::delete_info_embed (const dpp::snowflake &guild_id, const dpp::command_completion_event_t &callback)
{
    auto player = this->get_player (guild_id);
    if (!player)
        return false;

    auto pim = player->get_info_message ();

    if (pim.second != 0 || pim.first.is_source_message_deleted ())
        return false;

    auto mid = pim.first.id;
    auto cid = pim.first.channel_id;

    if (get_debug_state ())
        std::cerr << "[MANAGER::UPDATE_INFO_EMBED] Channel Info Embed Id Delete: " << cid << '\n';

    this->cluster->message_delete (mid, cid, [callback] (const dpp::confirmation_callback_t &res) { callback (res); });
    return true;
}

constexpr inline const char err_prefix[] = "[Manager::get_playing_info_embed ERROR] Failed to get_playing_info_embed";

/**
 * Statuses:
 * 0: success.
 * 1: No player.
 * 2: No track.
 *
 * -1: other error (dpp/logic error).
 */
std::pair<dpp::embed, int>
Manager::get_playing_info_embed (const dpp::snowflake &guild_id, bool force_playing_status, get_playing_info_embed_info_t *info_struct)
{
    try
        {
            auto guild_player = this->get_player (guild_id);
            if (!guild_player)
                return { {}, 1 };

            if (info_struct)
                {
                    info_struct->notification = guild_player->notification;
                    info_struct->stopped = guild_player->stopped;
                }

            // Reset shifted tracks
            guild_player->reset_shifted ();

            MCTrack track;
            MCTrack prev_track;
            MCTrack next_track;
            MCTrack skip_track;

            {
                auto siz = guild_player->queue.size ();
                if (!siz)
                    return { {}, 2 };

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

            std::string eaname;

            if (member_found)
                {
                    std::string nick = u.get_nickname ();

                    if (!nick.empty ())
                        {
                            eaname = nick;
                        }
                }

            if (eaname.empty ())
                {
                    if (uc)
                        eaname = uc->username;
                    else
                        eaname = "`[ERROR]` User not found";
                }

            dpp::embed_author ea;
            ea.name = eaname;

            std::string ua = member_found ? u.get_avatar_url (4096) : "";

            if (ua.empty () && uc)
                ua = uc->get_avatar_url (4096);

            if (!ua.empty ())
                ea.icon_url = ua;

            static const char *l_mode[] = { MUSICAT_U8 ("🔂"), MUSICAT_U8 ("🔁"), MUSICAT_U8 ("🔂🔁") };

            static const char *p_mode[] = { MUSICAT_U8 ("⏸️"), MUSICAT_U8 ("▶️") };

            const std::string et = mctrack::get_thumbnail (track);

            dpp::embed e;
            e.set_description (mctrack::get_description (track))
                .set_title (mctrack::get_title (track))
                .set_url (mctrack::get_url (track))
                .set_author (ea);

            if (!prev_track.raw.is_null ())
                e.add_field ("PREVIOUS", "[" + mctrack::get_title (prev_track) + "](" + mctrack::get_url (prev_track) + ")", true);

            if (!next_track.raw.is_null ())
                e.add_field ("NEXT", "[" + mctrack::get_title (next_track) + "](" + mctrack::get_url (next_track) + ")", true);

            if (!skip_track.raw.is_null ())
                e.add_field ("SKIP", "[" + mctrack::get_title (skip_track) + "](" + mctrack::get_url (skip_track) + ")");

            std::string ft = "";

            track_progress prog = util::get_track_progress (track);
            if (prog.status == 0)
                {
                    ft += "[" + format_duration (prog.current_ms) + "/" + format_duration (prog.duration) + "]";
                }

            bool has_p = false;

            auto *pc = guild_player->get_client ();
            if (pc)
                {
                    auto con = pc->get_voice (guild_id);
                    if (con && con->voiceclient && con->voiceclient->get_secs_remaining () > 0.05f)
                        {
                            has_p = true;
                            if (!ft.empty ())
                                ft += " ";

                            const char *m;
                            const char *m2;
                            bool playing = false;
                            if (con->voiceclient->is_paused ())
                                {
                                    m = p_mode[0];
                                    m2 = p_mode[1];
                                }
                            else
                                {
                                    m = p_mode[1];
                                    m2 = p_mode[0];
                                    playing = true;
                                }

                            ft += m;

                            if (info_struct)
                                {
                                    info_struct->play_pause_icon = m2;
                                    info_struct->playing = playing;
                                }
                        }
                }

            if (force_playing_status && !has_p)
                {
                    if (!ft.empty ())
                        ft += " ";

                    ft += p_mode[1];

                    if (info_struct)
                        {
                            info_struct->play_pause_icon = p_mode[0];
                            info_struct->playing = true;
                        }
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

                    ft += MUSICAT_U8 ("♾️");

                    if (guild_player->max_history_size)
                        ft += std::string (" (") + std::to_string (guild_player->max_history_size) + ")";
                }

            // !TODO: remove this when fully using ytdlp to support non-yt
            // tracks
            bool tinfo = !track.info.raw.is_null ();
            if (tinfo)
                {
                    if (!ft.empty ())
                        ft += " | ";

                    ft += std::string ("[") + std::to_string (track.info.average_bitrate ()) + "]";
                }

            int64_t rpt = guild_player->current_track.repeat;
            if (rpt > 0)
                {
                    if (!ft.empty ())
                        ft += " | ";

                    ft += std::string ("R ") + std::to_string (rpt);
                }

            int fx_count = guild_player->fx_get_active_count ();

            if (fx_count > 0)
                {
                    if (!ft.empty ())
                        ft += " | ";

                    ft += std::string ("FX") + (fx_count > 1 ? " (" + std::to_string (fx_count) + ")" : "");
                }

            if (!ft.empty ())
                e.set_footer (ft, "");

            if (color)
                e.set_color (color);

            if (!et.empty ())
                e.set_image (et);

            return { e, 0 };
        }
    catch (const dpp::exception &e)
        {
            fprintf (stderr, "%s [dpp::exception]: %s\n", err_prefix, e.what ());
        }
    catch (const std::logic_error &e)
        {
            fprintf (stderr, "%s [std::logic_error]: %s\n", err_prefix, e.what ());
        }

    return { {}, -1 };
}

namespace set_component
{
// first row
void
play_pause_button (dpp::component &c, const get_playing_info_embed_info_t &playback_info)
{
    c.set_id (playback_info.playing ? /*pause*/ ids.pause : /*resume*/ ids.resume).set_type (dpp::cot_button);

    if (playback_info.play_pause_icon)
        c.set_emoji (playback_info.play_pause_icon).set_style (dpp::cos_primary);
    else
        c.set_label (MUSICAT_U8 ("▶️/⏸️")).set_style (dpp::cos_secondary);
}

void
stop_button (dpp::component &c, const get_playing_info_embed_info_t &playback_info)
{

    c.set_type (dpp::cot_button)
        .set_style (dpp::cos_primary)
        .set_emoji (MUSICAT_U8 ("⏹"))
        // stop
        .set_id (ids.stop);

    if (playback_info.stopped)
        c.set_disabled (true);
}

void
loop_button (dpp::component &c)
{
    c.set_type (dpp::cot_button)
        .set_style (dpp::cos_primary)
        .set_emoji (MUSICAT_U8 ("🔃"))
        // loop
        .set_id (ids.loop);
}

void
shuffle_button (dpp::component &c)
{
    c.set_type (dpp::cot_button)
        .set_style (dpp::cos_primary)
        .set_emoji (MUSICAT_U8 ("🔀"))
        // shuffle
        .set_id (ids.shuffle);
}

void
expand_unexpand_button (dpp::component &c, bool expanded)
{
    c.set_type (dpp::cot_button)
        .set_style (dpp::cos_primary)
        .set_emoji (expanded ? MUSICAT_U8 ("⤴️") : MUSICAT_U8 ("⤵️"))

        .set_id (expanded ? ids.unexpand : ids.expand);
}

// expanded row
void
prev_button (dpp::component &c)
{
    c.set_type (dpp::cot_button)
        .set_style (dpp::cos_primary)
        .set_emoji (MUSICAT_U8 ("⏮️"))
        // prev
        .set_id (ids.prev);
}

void
rewind_button (dpp::component &c)
{
    c.set_type (dpp::cot_button)
        .set_style (dpp::cos_primary)
        .set_emoji (MUSICAT_U8 ("⏪"))
        // rewind
        .set_id (ids.rewind);
}

void
autoplay_button (dpp::component &c)
{
    c.set_type (dpp::cot_button)
        .set_style (dpp::cos_primary)
        .set_emoji (MUSICAT_U8 ("♾️"))
        // autoplay
        .set_id (ids.autoplay);
}

void
forward_button (dpp::component &c)
{
    c.set_type (dpp::cot_button)
        .set_style (dpp::cos_primary)
        .set_emoji (MUSICAT_U8 ("⏩"))
        // forward
        .set_id (ids.forward);
}

void
next_button (dpp::component &c)
{
    c.set_type (dpp::cot_button)
        .set_style (dpp::cos_primary)
        .set_emoji (MUSICAT_U8 ("⏭️"))
        // next
        .set_id (ids.next);
}

// second row
void
notif_button (dpp::component &c, bool is_enabled)
{
    c.set_emoji (is_enabled ? MUSICAT_U8 ("🔕") : MUSICAT_U8 ("🔔"))
        .set_id (is_enabled ? ids.disable_notif : ids.enable_notif)
        .set_type (dpp::cot_button)
        .set_style (dpp::cos_primary);
}

void
update_button (dpp::component &c)
{
    c.set_label ("Update")
        // update
        .set_id (ids.update)
        .set_type (dpp::cot_button)
        .set_style (dpp::cos_primary);
}
} // set_component

int
add_to_row (const dpp::component &c, const dpp::component *row[5], size_t &row_idx)
{
    if (row_idx >= 5)
        return 1;

    row[row_idx++] = &c;

    return 0;
}

int
compile_row_to_component (dpp::component &out, const dpp::component *row[5])
{
    for (size_t i = 0; i < 5; i++)
        {
            if (row[i] == NULL)
                break;

            out.add_component (*row[i]);
        }

    return 0;
}

namespace playing_info_utils
{
bool
is_button_expanded (const dpp::message &playing_info_message)
{
    if (!playing_info_message.components.empty () && playing_info_message.components.at (0).components.size () == 5)
        return playing_info_message.components.at (0).components.back ().custom_id == ids.unexpand;

    return false;
}
} // playing_info_utils

int
Manager::get_playing_info_message (dpp::message &msg, const dpp::snowflake &guild_id, bool force_playing_status, bool button_expanded)
{
    /**
     * Statuses:
     * get_playing_info_embed() statuses.
     */

    get_playing_info_embed_info_t playback_info;
    auto ge = get_playing_info_embed (guild_id, force_playing_status, &playback_info);

    if (ge.second != 0)
        return ge.second;

    const dpp::embed e = ge.first;

    // message components
    // \🔁 \🔂 \🔃 \🔀 \⏏️ \⏪ \⏩ \♾️ \⤵️ \⤴️ \⏮️ \⏭️ \🔕 \🔔
    const dpp::component *to_first_row[5] = { NULL };
    size_t to_first_row_idx = 0;

    dpp::component play_pause_btn;
    set_component::play_pause_button (play_pause_btn, playback_info);
    add_to_row (play_pause_btn, to_first_row, to_first_row_idx);

    dpp::component stop_btn;
    set_component::stop_button (stop_btn, playback_info);
    add_to_row (stop_btn, to_first_row, to_first_row_idx);

    dpp::component loop_btn;
    set_component::loop_button (loop_btn);
    add_to_row (loop_btn, to_first_row, to_first_row_idx);

    dpp::component shuffle_btn;
    set_component::shuffle_button (shuffle_btn);
    add_to_row (shuffle_btn, to_first_row, to_first_row_idx);

    dpp::component expand_unexpand_btn;
    set_component::expand_unexpand_button (expand_unexpand_btn, button_expanded);
    add_to_row (expand_unexpand_btn, to_first_row, to_first_row_idx);

    const dpp::component *to_second_row[5] = { NULL };
    size_t to_second_row_idx = 0;

    dpp::component notif_btn;
    set_component::notif_button (notif_btn, playback_info.notification);
    add_to_row (notif_btn, to_second_row, to_second_row_idx);

    dpp::component update_btn;
    set_component::update_button (update_btn);
    add_to_row (update_btn, to_second_row, to_second_row_idx);

    msg.embeds.clear ();
    msg.embeds.push_back (e);

    msg.components.clear ();

    dpp::component first_row, second_row;
    compile_row_to_component (first_row, to_first_row);
    compile_row_to_component (second_row, to_second_row);
    msg.add_component (first_row);

    if (button_expanded)
        {
            const dpp::component *to_expand_row[5] = { NULL };
            size_t to_expand_row_idx = 0;

            dpp::component prev_btn;
            set_component::prev_button (prev_btn);
            add_to_row (prev_btn, to_expand_row, to_expand_row_idx);

            dpp::component rewind_btn;
            set_component::rewind_button (rewind_btn);
            add_to_row (rewind_btn, to_expand_row, to_expand_row_idx);

            dpp::component autoplay_btn;
            set_component::autoplay_button (autoplay_btn);
            add_to_row (autoplay_btn, to_expand_row, to_expand_row_idx);

            dpp::component forward_btn;
            set_component::forward_button (forward_btn);
            add_to_row (forward_btn, to_expand_row, to_expand_row_idx);

            dpp::component next_btn;
            set_component::next_button (next_btn);
            add_to_row (next_btn, to_expand_row, to_expand_row_idx);

            dpp::component expanded_row;
            compile_row_to_component (expanded_row, to_expand_row);
            msg.add_component (expanded_row);
        }

    msg.add_component (second_row);

    return 0;
}

} // player
} // musicat
