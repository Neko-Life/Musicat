#include "musicat/cmds.h"
#include "musicat/yt-search.h"
#include "musicat/pagination.h"

namespace musicat {
    namespace command {
        namespace search {
            dpp::slashcommand get_register_obj(const dpp::snowflake& sha_id) {
                return dpp::slashcommand("search", "Search [for tracks]", sha_id)
                    .add_option(
                        dpp::command_option(
                            dpp::co_string,
                            "query",
                            "Search [this]",
                            true
                        )
                    );
            }

            dpp::interaction_modal_response modal_enqueue_searched_track() {
                dpp::component selmenu;
                selmenu.set_type(dpp::cot_text)
                    .set_label("Input track number to add to playback queue:")
                    .set_id("que_s_track")
                    .set_text_style(dpp::text_short);

                dpp::interaction_modal_response modal("modal_p", "Add Track");
                modal.add_component(selmenu);
                return modal;
            }

            void slash_run(const dpp::interaction_create_t& event) {
                std::string query = "";
                get_inter_param(event, "query", &query);

		event.thinking();

                auto res = yt_search::search(query);
                auto tracks = res.trackResults();

                size_t pl_siz = tracks.size();
                if (!pl_siz)
                {
                    event.edit_response("No result found");
                    return;
                }

                std::vector<dpp::embed> embeds = {};

                dpp::embed emb;
                std::string desc = "";
                size_t count = 0;
                size_t mult = 0;

                for (auto& t : tracks)
                {
                    size_t cn = mult * 10 + (++count);

                    desc += std::string("`")
                        + std::to_string(cn)
                        + "`: [" + t.title() + "]("
                        + t.url() + ") ["
                        + t.length() + "] - "
                        + t.channel().name
                        + "\n";

                    if (count == 10 || cn == pl_siz)
                    {
                        count = 0;
                        mult++;

                        emb.set_title(query)
                            .set_description(desc.length() > 2048 ? "Description exceeding char limit. rip" : desc);
                        embeds.emplace_back(emb);

                        emb = dpp::embed();
                        desc = "";
                    }
                }

                dpp::message m;
                m.add_embed(embeds.front());
                m.add_component(
                    dpp::component().add_component(
                        dpp::component()
                        .set_emoji(u8"🎵")
                        .set_label("Add Track")
                        .set_style(dpp::cos_success)
                        .set_id("modal_p/que_s_track")
                    )
                );

                bool paginate = embeds.size() > 1;
                if (paginate) paginate::add_pagination_buttons(&m);

                std::any storage_data = tracks;
                event.edit_response(m, paginate::get_inter_reply_cb(event, paginate, event.from->creator, embeds, storage_data));
            }
        }
    }
}
