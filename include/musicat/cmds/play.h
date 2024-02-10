#ifndef MUSICAT_COMMAND_PLAY_H
#define MUSICAT_COMMAND_PLAY_H

#include "musicat/player.h"
#include <dpp/dpp.h>

namespace musicat::command::play
{
namespace autocomplete
{
void query (const dpp::autocomplete_t &event, const std::string &param);
} // autocomplete

dpp::slashcommand get_register_obj (const dpp::snowflake &sha_id);
void slash_run (const dpp::slashcommand_t &event);

std::pair<player::MCTrack, int>
find_track (const bool playlist, const std::string &arg_query,
            player::player_manager_ptr_t player_manager,
            const dpp::snowflake guild_id, const bool no_check_history = false,
            const std::string &cache_id = "");

std::string get_filename_from_result (player::MCTrack &result);

std::pair<bool, int> track_exist (const std::string &fname,
                                  const std::string &url,
                                  player::player_manager_ptr_t player_manager,
                                  bool from_interaction,
                                  dpp::snowflake guild_id,
                                  bool no_download = false);

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
 * @param cache_id Id to search in cache
 */
void add_track (bool playlist, dpp::snowflake guild_id, std::string arg_query,
                int64_t arg_top, bool vcclient_cont, dpp::voiceconn *v,
                const dpp::snowflake channel_id, const dpp::snowflake sha_id,
                bool from_interaction, dpp::discord_client *from,
                const dpp::interaction_create_t event
                = dpp::interaction_create_t (NULL, "{}"),
                bool continued = false, int64_t arg_slip = 0,
                const std::string &cache_id = "");

/**
 * @brief Decide whether the client need to play or not at its current state
 * @param from
 * @param guild_id
 * @param continued
 */
void decide_play (dpp::discord_client *from, const dpp::snowflake &guild_id,
                  const bool &continued);
} // musicat::command::play

#endif // MUSICAT_COMMAND_PLAY_H
