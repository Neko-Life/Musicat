#include "musicat/cmds/search.h"
#include "musicat/YTDLPTrack.h"
#include "musicat/function_macros.h"
#include "musicat/mctrack.h"
#include "musicat/musicat.h"
#include "musicat/pagination.h"
#include "musicat/thread_manager.h"
#include "musicat/util.h"
#include "musicat/util_response.h"

namespace musicat::command::search
{
dpp::slashcommand
get_register_obj (const dpp::snowflake &sha_id)
{
    return dpp::slashcommand ("search", "Search [for tracks]", sha_id)
        .add_option (dpp::command_option (dpp::co_string, "query",
                                          "Search [this]", true)
                         .set_max_length (150));
}

dpp::interaction_modal_response
modal_enqueue_searched_track ()
{
    dpp::component input = util::response::create_short_text_input (
        "Input track number to add to playback queue:", "que_s_track");

    dpp::interaction_modal_response modal ("modal_p", "Add Track");
    modal.add_component (input);
    return modal;
}

dpp::interaction_modal_response
modal_enqueue_searched_track_top ()
{
    dpp::component input = util::response::create_short_text_input (
        "Input track number to add to top of queue:", "que_s_track_top");

    dpp::interaction_modal_response modal ("modal_p", "Add Top");
    modal.add_component (input);
    return modal;
}

dpp::interaction_modal_response
modal_enqueue_searched_track_slip ()
{
    dpp::component input1 = util::response::create_short_text_input (
        "Input track number to add to playback queue:", "que_s_track");
    dpp::component input2 = util::response::create_short_text_input (
        "Input position to slip into playback queue:", "que_s_track_slip");

    dpp::interaction_modal_response modal ("modal_p", "Add Slip");
    modal.add_component (input1);
    modal.add_row ();
    modal.add_component (input2);
    return modal;
}

std::string
search_entry_append_track_info (const player::MCTrack &track)
{
    if (mctrack::is_short (track))
        return " [SHORT]";

    return " [" + mctrack::get_length_str (track) + "] - "
           + mctrack::get_channel_name (track);
}

void
slash_run (const dpp::slashcommand_t &event)
{
    std::thread rt ([event] () {
        thread_manager::DoneSetter tmds;

        std::string query = "";
        get_inter_param (event, "query", &query);

        if (query.empty ())
            return event.reply ("No query?");

        event.thinking ();
        nlohmann::json res;

        try
            {
                res = mctrack::fetch (
                    { query, YDLP_DEFAULT_MAX_ENTRIES, false });
            }
        catch (std::exception &e)
            {
                std::cerr << "[command::search::slash_run ERROR] "
                          << event.command.guild_id << ':' << e.what ()
                          << '\n';

                event.edit_response (
                    "`[FATAL]` Error while searching for query");

                return;
            }

        auto tracks = YTDLPTrack::get_playlist_entries (res);

        size_t pl_siz = tracks.size ();
        if (!pl_siz)
            {
                event.edit_response ("No result found");
                return;
            }

        std::vector<dpp::embed> embeds = {};

        dpp::embed emb;
        std::string desc = "";
        size_t count = 0;
        size_t mult = 0;

        for (auto &t : tracks)
            {
                size_t cn = mult * 10 + (++count);
                const std::string tit = mctrack::get_title (t);
                std::string tit_char;
                util::u8_limit_length (tit.c_str (), tit_char, 80);

                desc += std::string ("`") + std::to_string (cn) + "`: ["
                        + tit_char
                        + (tit.length () > tit_char.length () ? "..." : "")
                        + "](" + mctrack::get_url (t) + ")"
                        + search_entry_append_track_info (t) + "\n";

                if (count == 10 || cn == pl_siz)
                    {
                        count = 0;
                        mult++;

                        emb.set_title (query).set_description (
                            desc.length () > 2048
                                ? "Description exceeding char limit. rip"
                                : desc);

                        embeds.emplace_back (emb);

                        emb = dpp::embed ();
                        desc = "";
                    }
            }

        dpp::message m;
        m.add_embed (embeds.front ());
        m.add_component (
            dpp::component ()
                .add_component (dpp::component ()
                                    .set_emoji (MUSICAT_U8 ("🎵"))
                                    .set_label ("Add Track")
                                    .set_style (dpp::cos_success)
                                    .set_id ("modal_p/que_s_track"))
                .add_component (dpp::component ()
                                    .set_emoji (MUSICAT_U8 ("🎵"))
                                    .set_label ("Add Slip")
                                    .set_style (dpp::cos_primary)
                                    .set_id ("modal_p/que_s_track_slip"))
                .add_component (dpp::component ()
                                    .set_emoji (MUSICAT_U8 ("🎵"))
                                    .set_label ("Add Top")
                                    .set_style (dpp::cos_danger)
                                    .set_id ("modal_p/que_s_track_top")));

        bool paginate = embeds.size () > 1;
        if (paginate)
            paginate::add_pagination_buttons (&m);

        std::any storage_data = tracks;
        event.edit_response (m, paginate::get_inter_reply_cb (
                                    event, paginate, event.from->creator,
                                    embeds, storage_data));
    });

    thread_manager::dispatch (rt);
}
} // musicat::command::search
