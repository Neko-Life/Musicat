#include "musicat/cmds.h"
#include "musicat/yt-search.h"

namespace musicat_command {
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

        void slash_run(const dpp::interaction_create_t& event) {
            string query = "";
            mc::get_inter_param(event, "query", &query);

            auto res = yt_search::search(query);
            const auto tracks = res.trackResults();
            if (!tracks.size())
            {
                event.reply("No result found");
                return;
            }

            event.reply("Never gonna give you up!");
        }
    }
}
