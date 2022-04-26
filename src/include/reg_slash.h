#include <dpp/dpp.h>
#include "musicat.h"

using string = std::string;

namespace sha_slash_cmd {
    std::vector<dpp::slashcommand> get_all(dpp::snowflake sha_id) {
        std::vector<dpp::slashcommand> slash_commands = {
            {
                dpp::slashcommand(
                    "play",
                    "Play [a song], resume (paused playback), or find and add [song] to [queue]",
                    sha_id
                ).add_option(
                    dpp::command_option(
                        dpp::co_string,
                        "query",
                        "Song to search or Youtube URL to play",
                        true
                    ).set_auto_complete(true)
                )
            }
        };
        return slash_commands;
    }
}
