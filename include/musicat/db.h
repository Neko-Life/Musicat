#ifndef MUSICAT_DATABASE_H
#define MUSICAT_DATABASE_H

#include <string>
#include <libpq-fe.h>
#include "musicat/player.h"

#define ERRBUFSIZE 256

namespace musicat {
    /**
     * @brief Almost every function call locks and unlock a single mutex.
     *
     */
    namespace database {

        enum get_user_playlist_type : uint8_t {
            gup_all,
            gup_name_only,
            gup_raw_only,
            gup_ts_only,
        };

        // -----------------------------------------------------------------------
        // INSTANCE MANIPULATION
        // -----------------------------------------------------------------------

        /**
         * @brief Initialize database, load config and connecting to server
         *
         * @param _conninfo Connection param
         * @return ConnStatusType Return conn status, 0 (CONNECTION_OK) on sucess
         */
        ConnStatusType init(const std::string& _conninfo);

        /**
         * @brief Cancel currently running query
         *
         * @return int 1 on success, else 0
         */
        int cancel();

        /**
         * @brief Destroy database instance, must be called to free the memory (eg. before exiting program)
         *
         */
        void shutdown();

        /**
         * @brief Reconnect database, setting the conninfo with new one if provided
         *
         * @param force Whether to force even it's not needed to reconnect
         * @param _conninfo Conn param
         * @return ConnStatusType Return CONNECTION_OK on success or not needing to reconnect
         */
        ConnStatusType reconnect(const bool& force = false, const std::string& _conninfo = "");

        /**
         * @brief Get current connect param
         *
         * @return std::string
         */
        const std::string get_connect_param() noexcept(true);

        /**
         * @brief Obtained PGresult* struct should always be cleared with this function
         *
         * @param res
         * @param status
         * @return ExecStatusType
         */
        ExecStatusType finish_res(PGresult* res, ExecStatusType status = PGRES_COMMAND_OK);

        /**
         * @brief Validate str for db playlist name, a check is a must to minimize SQL injection attack success rate
         *
         * @param str
         * @return true
         * @return false
         */
        bool valid_name(const std::string& str);

	/**
	 * @brief Convert playlist to json
	 *
	 * @param playlist Playlist to convert to json
	 * @return nlohmann::json An array of track from the playlist
	 */
	nlohmann::json
	    convert_playlist_to_json(const std::deque<player::MCTrack>& playlist);

        // -----------------------------------------------------------------------
        // TABLE MANIPULATION
        // -----------------------------------------------------------------------

        /**
         * @brief Create new playlist table for new user
         *
         * @param user_id
         * @return ExecStatusType PGRES_COMMAND_OK on success, -1 if user_id is 0
         */
        ExecStatusType create_table_playlist(const dpp::snowflake& user_id);

	/**
	 * @brief Create guild_current_queue table
	 *
	 * @return ExecStatusType PGRES_COMMAND_OK on success
	 */
	ExecStatusType create_table_guilds_current_queue();

        /**
         * @brief Get all user playlist, result must be freed using finish_res()
         *
         * @param user_id
         * @param type
         * @return std::pair<PGresult*, ExecStatusType> Return code -1 if user_id is 0, -2 if invalid type
         */
        std::pair<PGresult*, ExecStatusType>
            get_all_user_playlist(const dpp::snowflake& user_id, const get_user_playlist_type type = gup_all);

        /**
         * @brief Get one user playlist with id `name`, result must be freed using finish_res()
         *
         * @param user_id
         * @param name
         * @param type
         * @return std::pair<PGresult*, ExecStatusType> Return code -1 if user_id is 0, -2 if invalid type,
	 * -3 if invalid name format
         */
        std::pair<PGresult*, ExecStatusType>
            get_user_playlist(const dpp::snowflake& user_id,
                const std::string& name,
                const get_user_playlist_type type = gup_raw_only);

	/**
	 * @brief Construct std::deque containing tracks from PGresult object, 
	 * 	this function doesn't free the PGresult object.
	 *
	 * @param res PGresult object
	 * @return std::pair<std::deque<player::MCTrack>, int> code -1 if json is null, -2 if no row found in the table else 0
	 */
	std::pair<std::deque<player::MCTrack>, int>
	    get_playlist_from_PGresult(PGresult *res);

        /**
         * @brief Update user playlist with id `name`, can be insertion or an update
         *
         * @param user_id
         * @param name
         * @param playlist
         * @return ExecStatusType -1 if user_id is 0 else PGRES_COMMAND_OK
         */
        ExecStatusType update_user_playlist(const dpp::snowflake& user_id,
            const std::string& name,
            const std::deque<player::MCTrack>& playlist);

        /**
         * @brief Delete user playlist with id `name`
         *
         * @param user_id
         * @param name
         * @return ExecStatusType PGRES_FATAL_ERROR if row not found in table or table not found, -1 if user_id is 0 else PGRES_TUPLES_OK
	 */
        ExecStatusType delete_user_playlist(const dpp::snowflake& user_id, const std::string& name);

	/**
	 * @brief Update guild current queue in table
	 *
	 * @param guild_id
	 * @param playlist New queue
	 * @return ExecStatusType -1 if guild_id is 0, -2 if playlist have no size
	 * 			-3 if failed to create new table, PGRES_COMMAND_OK on success
	 */
	ExecStatusType
	    update_guild_current_queue(const dpp::snowflake& guild_id, const std::deque<player::MCTrack>& playlist);
	
	/**
	 * @brief Delete guild current queue row from table
	 * 
	 * @param guild_id
	 * @return ExecStatusType PGRES_FATAL_ERROR if row not found in table,-1 if guild_id is 0 else PGRES_TUPLES_OK
	 */
	ExecStatusType
	    delete_guild_current_queue(const dpp::snowflake& guild_id);
    }
}

#endif // MUSICAT_DATABASE_H
