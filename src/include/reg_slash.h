#include <dpp/dpp.h>
#include "musicat.h"

using string = std::string;

namespace sha_slash_cmd {
    std::vector<dpp::slashcommand> get_all(dpp::snowflake sha_id) {
        std::vector<dpp::slashcommand> slash_commands = {
            {
                dpp::slashcommand(
                    "play",
                    "Play [a song], resume [paused playback], or add [song to queue]",
                    sha_id
                ).add_option(
                    dpp::command_option(
                        dpp::co_string,
                        "query",
                        "Song [to search] or Youtube URL [to play]"
                    ).set_auto_complete(true)
                ).add_option(
                    dpp::command_option(
                        dpp::co_integer,
                        "top",
                        "Add [this song] to the top [of the queue]"
                    ).add_choice(
                        dpp::command_option_choice("Yes", 1)
                    ).add_choice(
                        dpp::command_option_choice("No", 0)
                    )
                )
            },
            {
                dpp::slashcommand(
                    "skip",
                    "Skip [currently playing] song",
                    sha_id
                ).add_option(
                    dpp::command_option(
                        dpp::co_integer,
                        "amount",
                        "How many [song] to skip"
                    )
                )
            },
            {
                dpp::slashcommand(
                    "pause",
                    "Pause [currently playing] song",
                    sha_id
                )
            },
            {
                dpp::slashcommand(
                    "loop",
                    "Configure [repeat] mode",
                    sha_id
                ).add_option(
                    dpp::command_option(
                        dpp::co_integer,
                        "mode",
                        "Set [to this] mode",
                        true
                    ).add_choice(
                        dpp::command_option_choice("One", 1)
                    ).add_choice(
                        dpp::command_option_choice("Queue", 2)
                    ).add_choice(
                        dpp::command_option_choice("One/Queue", 3)
                    ).add_choice(
                        dpp::command_option_choice("Off", 0)
                    )
                )
            }
        };
        return slash_commands;
    }
}
