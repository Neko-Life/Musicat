#include "musicat/cmds.h"
#include "musicat/pagination.h"
#include "musicat/yt-search.h"
#include "musicat/util.h"
#include <dpp/message.h>
#include <string>

namespace musicat
{
namespace command
{
namespace search
{
// =============== PRIVATE ==================

dpp::component
create_short_text_input (const std::string &label, const std::string &id)
{
    dpp::component component;
    component.set_type (dpp::cot_text)
        .set_label (label)
        .set_id (id)
        .set_text_style (dpp::text_short);
    return component;
}

// =============== PRIVATE END ==================

dpp::slashcommand
get_register_obj (const dpp::snowflake &sha_id)
{
    return dpp::slashcommand ("search", "Search [for tracks]", sha_id)
        .add_option (dpp::command_option (dpp::co_string, "query",
                                          "Search [this]", true));
}

dpp::interaction_modal_response
modal_enqueue_searched_track ()
{
    dpp::component input = create_short_text_input ("Input track number to add to playback queue:",
                                                    "que_s_track");

    dpp::interaction_modal_response modal ("modal_p", "Add Track");
    modal.add_component (input);
    return modal;
}

dpp::interaction_modal_response
modal_enqueue_searched_track_top ()
{
    dpp::component input = create_short_text_input ("Input track number to add to top of queue:",
                                                   "que_s_track_top");

    dpp::interaction_modal_response modal ("modal_p", "Add Top");
    modal.add_component (input);
    return modal;
}

dpp::interaction_modal_response
modal_enqueue_searched_track_slip ()
{
    dpp::component input1 = create_short_text_input ("Input track number to add to playback queue:",
                                                     "que_s_track");
    dpp::component input2 = create_short_text_input ("Input position to slip into playback queue:",
                                                    "que_s_track_slip");

    dpp::interaction_modal_response modal ("modal_p", "Add Slip");
    modal.add_component (input1);
    modal.add_row();
    modal.add_component (input2);
    return modal;
}

void
slash_run (const dpp::interaction_create_t &event)
{
    std::string query = "";
    get_inter_param (event, "query", &query);

    event.thinking ();

    auto res = yt_search::search (query);
    auto tracks = res.trackResults ();

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
            const std::string tit = t.title ();
            char tit_char[512];
            util::u8_limit_length(tit.c_str(), tit_char, 80);

            desc += std::string ("`") + std::to_string (cn) + "`: ["
                    + tit_char + (tit.length () > 80 ? "..." : "")
                    + "](" + t.url () + ") [" + t.length () + "] - "
                    + t.channel ().name + "\n";

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
                                .set_emoji (u8"🎵")
                                .set_label ("Add Track")
                                .set_style (dpp::cos_success)
                                .set_id ("modal_p/que_s_track"))
            .add_component (dpp::component ()
                                .set_emoji (u8"🎵")
                                .set_label ("Add Slip")
                                .set_style (dpp::cos_primary)
                                .set_id ("modal_p/que_s_track_slip"))
            .add_component (dpp::component ()
                                .set_emoji (u8"🎵")
                                .set_label ("Add Top")
                                .set_style (dpp::cos_danger)
                                .set_id ("modal_p/que_s_track_top")));

    bool paginate = embeds.size () > 1;
    if (paginate)
        paginate::add_pagination_buttons (&m);

    std::any storage_data = tracks;
    event.edit_response (
        m, paginate::get_inter_reply_cb (event, paginate, event.from->creator,
                                         embeds, storage_data));
}
} // search
} // command
} // musicat
