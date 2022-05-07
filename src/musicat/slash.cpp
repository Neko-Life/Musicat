#include "musicat/slash.h"
#include "musicat/player.h"

namespace mpl = musicat_player;

namespace musicat_slash {
    using string = std::string;
    std::vector<dpp::slashcommand> get_all(dpp::snowflake sha_id) {
        std::vector<dpp::slashcommand> slash_commands({
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
                        dpp::command_option_choice("One", mpl::loop_mode_t::l_song)
                    ).add_choice(
                        dpp::command_option_choice("Queue", mpl::loop_mode_t::l_queue)
                    ).add_choice(
                        dpp::command_option_choice("One/Queue", mpl::loop_mode_t::l_song_queue)
                    ).add_choice(
                        dpp::command_option_choice("Off", mpl::loop_mode_t::l_none)
                    )
                )
            },
            {
                dpp::slashcommand(
                    "queue",
                    "Show or modify [tracks in the] queue",
                    sha_id
                ).add_option(
                    dpp::command_option(
                        dpp::co_integer,
                        "action",
                        "Modify [tracks in the] queue"
                    ).add_choice(
                        dpp::command_option_choice("Shuffle", queue_modify_t::m_shuffle)
                    ).add_choice(
                        dpp::command_option_choice("Reverse", queue_modify_t::m_reverse)
                    ).add_choice(
                        dpp::command_option_choice("Clear Left", queue_modify_t::m_clear_left)
                    ).add_choice(
                        dpp::command_option_choice("Clear", queue_modify_t::m_clear)
                    )
                )
            }
        });
        return slash_commands;
    }
}
