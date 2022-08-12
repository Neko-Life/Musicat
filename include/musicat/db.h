#ifndef MUSICAT_DATABASE_H
#define MUSICAT_DATABASE_H

#include <string>
#include <libpq-fe.h>

#define ERRBUFSIZE 256

namespace musicat {
    /**
     * @brief Almost every function call locks and unlock a single mutex.
     *
     */
    namespace database {

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
         * @brief Validate str for db playlist name
         *
         * @param str
         * @return true
         * @return false
         */
        bool valid_name(const std::string& str);

        // -----------------------------------------------------------------------
        // TABLE MANIPULATION
        // -----------------------------------------------------------------------

        /**
         * @brief Create new playlist table for new user
         *
         * @param user_id
         * @return ExecStatusType -1 if user_id is 0
         */
        ExecStatusType create_table_playlist(const dpp::snowflake& user_id);

        /**
         * @brief Get all user playlist, result must be freed using finish_res()
         *
         * @param user_id
         * @return std::pair<PGresult*, ExecStatusType> Return {nullptr,-1} if user_id is 0
         */
        std::pair<PGresult*, ExecStatusType>
            get_all_user_playlist(const dpp::snowflake& user_id);
    }
}

#endif // MUSICAT_DATABASE_H
