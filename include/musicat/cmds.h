#ifndef MUSICAT_COMMAND_H
#define MUSICAT_COMMAND_H

#include "musicat/musicat.h"
#include "musicat/player.h"
#include <dpp/dpp.h>
#include <mutex>
#include <string>
#include <vector>

namespace musicat
{
namespace command
{
using command_handlers_map_t
    = std::map<std::string, void (*) (const dpp::slashcommand_t &)>;

struct handle_command_params_t
{
    const std::string &command_name;
    const command_handlers_map_t &command_handlers_map;
    const dpp::slashcommand_t &event;
};

enum handle_command_status_e
{
    HANDLE_SLASH_COMMAND_SUCCESS,
    HANDLE_SLASH_COMMAND_NO_HANDLER,
};

handle_command_status_e handle_command (const handle_command_params_t &params);

///////////////////////////////////////////////////////////////////

namespace hello
{
dpp::slashcommand get_register_obj (const dpp::snowflake &sha_id);
void slash_run (const dpp::slashcommand_t &event);
} // hello

namespace invite
{
dpp::slashcommand get_register_obj (const dpp::snowflake &sha_id);
void slash_run (const dpp::slashcommand_t &event);
} // invite

namespace pause
{
dpp::slashcommand get_register_obj (const dpp::snowflake &sha_id);
void slash_run (const dpp::slashcommand_t &event);
} // pause

namespace skip
{
dpp::slashcommand get_register_obj (const dpp::snowflake &sha_id);
void slash_run (const dpp::slashcommand_t &event);
} // skip

namespace play
{
namespace autocomplete
{
void query (const dpp::autocomplete_t &event, std::string param,
            player::player_manager_ptr player_manager);
}

dpp::slashcommand get_register_obj (const dpp::snowflake &sha_id);
void slash_run (const dpp::slashcommand_t &event);

std::pair<yt_search::YTrack, int>
find_track (bool playlist, std::string &arg_query,
            player::player_manager_ptr player_manager, bool from_interaction,
            dpp::snowflake guild_id, bool no_check_history = false);

std::string get_filename_from_result (yt_search::YTrack &result);

std::pair<bool, int>
track_exist (const std::string &fname, const std::string &url,
             player::player_manager_ptr player_manager, bool from_interaction,
             dpp::snowflake guild_id, bool no_download = false);

/**
 * @brief Search and add track to guild queue, can be used for interaction and
 * non interaction. Interaction must have already deferred/replied.
 *
 * !TODO: WHAT IN THE WORLD WAS THIS???
 *
 * @param playlist Whether arg_query is youtube playlist url or search query
 * @param guild_id Guild which data to be updated with
 * @param arg_query Valid youtube url or search query
 * @param arg_top Whether to add the track to the top of the queue or not
 * @param vcclient_cont Whether client is in a voice channel or not
 * @param v Voice connection, can be NULL
 * @param channel_id Target voice channel for the client to join and play
 * tracks to
 * @param sha_id Client user Id
 * @param from_interaction Whether from an interaction or not
 * @param from Discord client used to reconnect/join voice channel
 * @param event Can be incomplete type or filled if from interaction
 * @param continued Whether marker to initialize playback has been inserted
 */
void add_track (bool playlist, dpp::snowflake guild_id, std::string arg_query,
                int64_t arg_top, bool vcclient_cont, dpp::voiceconn *v,
                const dpp::snowflake channel_id, const dpp::snowflake sha_id,
                bool from_interaction, dpp::discord_client *from,
                const dpp::interaction_create_t event
                = dpp::interaction_create_t (NULL, "{}"),
                bool continued = false, int64_t arg_slip = 0);

/**
 * @brief Decide whether the client need to play or not at its current state
 * @param from
 * @param guild_id
 * @param continued
 */
void decide_play (dpp::discord_client *from, const dpp::snowflake &guild_id,
                  const bool &continued);
} // play

namespace loop
{
dpp::slashcommand get_register_obj (const dpp::snowflake &sha_id);
void slash_run (const dpp::slashcommand_t &event);
} // loop

namespace queue
{
enum queue_modify_t : int8_t
{
    // Shuffle the queue
    m_shuffle,
    // Reverse the queue
    m_reverse,
    // Clear songs added by users who left the vc
    m_clear_left,
    // Clear queue
    m_clear,
    // clear songs added by musicat
    m_clear_musicat
};

dpp::slashcommand get_register_obj (const dpp::snowflake &sha_id);
dpp::coroutine<void> slash_run (const dpp::slashcommand_t &event);
} // queue

namespace autoplay
{
dpp::slashcommand get_register_obj (const dpp::snowflake &sha_id);
void slash_run (const dpp::slashcommand_t &event);
} // autoplay

namespace move
{
dpp::slashcommand get_register_obj (const dpp::snowflake &sha_id);
void slash_run (const dpp::slashcommand_t &event);
} // move

namespace remove
{
dpp::slashcommand get_register_obj (const dpp::snowflake &sha_id);
void slash_run (const dpp::slashcommand_t &event);
} // remove

namespace bubble_wrap
{
dpp::slashcommand get_register_obj (const dpp::snowflake &sha_id);
void slash_run (const dpp::slashcommand_t &event);
} // bubble_wrap

namespace search
{
dpp::slashcommand get_register_obj (const dpp::snowflake &sha_id);

dpp::interaction_modal_response modal_enqueue_searched_track ();
dpp::interaction_modal_response modal_enqueue_searched_track_top ();
dpp::interaction_modal_response modal_enqueue_searched_track_slip ();

void slash_run (const dpp::slashcommand_t &event);
} // search

namespace playlist
{
namespace autocomplete
{
void id (const dpp::autocomplete_t &event, std::string param);
}

namespace save
{
dpp::command_option get_option_obj ();
void slash_run (const dpp::slashcommand_t &event);
}

namespace load
{
dpp::command_option get_option_obj ();
void slash_run (const dpp::slashcommand_t &event);
}

namespace view
{
dpp::command_option get_option_obj ();
void slash_run (const dpp::slashcommand_t &event);
}

namespace delete_
{
dpp::command_option get_option_obj ();
void slash_run (const dpp::slashcommand_t &event);
}

dpp::slashcommand get_register_obj (const dpp::snowflake &sha_id);
void slash_run (const dpp::slashcommand_t &event);
} // playlist

// OMGGG THEY WON'T STOP ASKING ME TO MAKE THIS OMIGOD, am making this for
// friends, don't take it the other way around!
namespace stop
{
dpp::slashcommand get_register_obj (const dpp::snowflake &sha_id);
void slash_run (const dpp::slashcommand_t &event);
} // stop

namespace interactive_message
{
namespace create
{
dpp::command_option get_option_obj ();
void slash_run (const dpp::slashcommand_t &event);
}

dpp::slashcommand get_register_obj (const dpp::snowflake &sha_id);
void slash_run (const dpp::slashcommand_t &event);
} // interactive_message

namespace join
{
dpp::slashcommand get_register_obj (const dpp::snowflake &sha_id);
void slash_run (const dpp::slashcommand_t &event);
} // join

namespace leave
{
dpp::slashcommand get_register_obj (const dpp::snowflake &sha_id);
void slash_run (const dpp::slashcommand_t &event);
} // leave

namespace download
{
namespace autocomplete
{
void track (const dpp::autocomplete_t &event, std::string param,
            player::player_manager_ptr player_manager);
}

dpp::slashcommand get_register_obj (const dpp::snowflake &sha_id);
void slash_run (const dpp::slashcommand_t &event);
} // download

namespace image
{
namespace autocomplete
{
void type (const dpp::autocomplete_t &event, std::string param);
}

dpp::slashcommand get_register_obj (const dpp::snowflake &sha_id);
void slash_run (const dpp::slashcommand_t &event);
} // image

namespace seek
{
dpp::slashcommand get_register_obj (const dpp::snowflake &sha_id);
void slash_run (const dpp::slashcommand_t &event);
} // seek

namespace progress
{
dpp::slashcommand get_register_obj (const dpp::snowflake &sha_id);
void slash_run (const dpp::slashcommand_t &event);

void update_progress (const dpp::button_click_t &event);
} // progress

namespace volume
{
dpp::slashcommand get_register_obj (const dpp::snowflake &sha_id);
void slash_run (const dpp::slashcommand_t &event);
} // volume

namespace filters
{

namespace equalizer
{
void show_setting (const dpp::slashcommand_t &event);

} // equalizer

} // filters

} // command
} // musicat

#endif // MUSICAT_COMMAND_H
