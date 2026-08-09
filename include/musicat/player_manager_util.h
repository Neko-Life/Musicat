#ifndef SHA_PLAYER_MANAGER_UTIL_H
#define SHA_PLAYER_MANAGER_UTIL_H

#include <dpp/dpp.h>
#include <memory>
#include <vector>

namespace musicat::player
{

#ifndef SHA_PLAYER_H
#warning Missing #include "musicat/player.h"
class guild_player_t;
#endif // SHA_PLAYER_H

#ifndef MUSICAT_MCTRACK_H
#warning Missing #include "musicat/mctrack.h"
struct MCTrack;
#endif // MUSICAT_MCTRACK_H

struct track_progress
{
    int64_t current_ms;
    int64_t duration;
    int status;
};

struct gat_t
{
    // Stripped .opus extension
    std::string name;
    std::string fullname;
    std::string fullpath;
    // File size in bytes
    // Will be 0 with with_size = false
    size_t size;
    // Timestamp when file last accessed
    // Will be 0 with with_size = false
    time_t last_access;
};

/**
 * @brief Get all available track to use, locks a mutex since readdir is
 *        not thread safe
 * @param amount Amount of track to return
 * @param with_stat Stat the file
 *
 * @return std::vector<gat_t>
 */
std::vector<gat_t> get_available_tracks (const size_t &amount = 0, bool with_stat = false);

void control_music_cache (const size_t size_limit);

// ================================================================================

int get_track_failed_playback_count (const std::string &filename);
int set_track_failed_playback_count (const std::string &filename, int c);

// ================================================================================

// see if the next call to find_track() will block for a while
bool find_track_will_block ();

std::pair<MCTrack, int> find_track (const bool playlist, const std::string &arg_query, const dpp::snowflake guild_id,
                                    const bool no_check_history = false, const std::string &cache_id = "");

std::string get_filename_from_result (MCTrack &result);

std::pair<bool, int> track_exist (const std::string &fname, const std::string &url, bool from_interaction, dpp::snowflake guild_id,
                                  bool no_download = false);

// see if next call to run_download_thread() will block
bool run_download_thread_will_block (const MCTrack &result, const std::string &fname);

/**
 * @brief Default download thread for search and add track to guild queue, can be used for interaction and
 * non interaction. Interaction must have already deferred/replied.
 *
 * @param shard_id Shard ID for the Discord client
 * @param sha_id Client user Id
 * @param dling Whether currently downloading or not, used for interaction response content
 * @param fname Path to file
 * @param arg_top Whether to add the track to the top of the queue or not
 * @param from_interaction Whether from an interaction or not
 * @param guild_id Guild which data to be updated with
 * @param continued Whether marker to initialize playback has been inserted
 * @param arg_slip Slip track into position in the queue
 * @param event Can be incomplete type or filled if from interaction, used for interaction response if from interaction
 * @param result Search result for the track, used for interaction response if from interaction and for getting filename if not provided in
 * argument
 * @param downloaded_response Response content to send when download finished if from interaction, ignored otherwise
 */
void run_download_thread (const uint32_t shard_id, const dpp::snowflake &sha_id, const bool dling, const std::string &fname,
                          const bool arg_top, const bool from_interaction, const dpp::snowflake &guild_id, const bool continued,
                          const int64_t arg_slip, const dpp::interaction_create_t &event, const MCTrack &result,
                          const std::string &downloaded_response);

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
 * @param shard_id Shard ID for the Discord client
 * @param event Can be incomplete type or filled if from interaction
 * @param continued Whether marker to initialize playback has been inserted
 * @param cache_id Id to search in cache
 */
void add_track (bool playlist, dpp::snowflake guild_id, std::string arg_query, int64_t arg_top, bool vcclient_cont, dpp::voiceconn *v,
                const dpp::snowflake channel_id, const dpp::snowflake sha_id, bool from_interaction, const uint32_t shard_id,
                const dpp::interaction_create_t event = dpp::interaction_create_t (NULL, 0, "{}"), bool continued = false,
                int64_t arg_slip = 0, const std::string &cache_id = "");

/**
 * @brief Decide whether the client need to play or not at its current state
 * @param from
 * @param guild_id
 * @param continued
 */
void decide_play (dpp::discord_client *from, const dpp::snowflake &guild_id, const bool &continued);

// ================================================================================

namespace playing_info_utils
{
bool is_button_expanded (const dpp::message &playing_info_message);
} // playing_info_utils

/////////////////////////////////////////////////////////////////////////////////////
} // namespace musicat::player

namespace musicat::util
{

/**
 * @brief Check if guild player has current track loaded
 */
bool player_has_current_track (std::shared_ptr<player::guild_player_t> guild_player);

/**
 * @brief Get track current progress in ms
 */
player::track_progress get_track_progress (const player::MCTrack &track);

void set_playback_info_track_data (nlohmann::json &data, const dpp::snowflake &guild_id, /* const */ player::MCTrack &track);
nlohmann::json get_playback_info_json (const dpp::snowflake &guild_id);

} // util

#endif // SHA_PLAYER_MANAGER_UTIL_H
