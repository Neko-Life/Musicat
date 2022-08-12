#include <libpq-fe.h>
#include <string>
#include <mutex>
#include <string.h>
#include <regex>
#include <dpp/dpp.h>
#include "nlohmann/json.hpp"
#include "musicat/db.h"

namespace musicat {
    namespace database {
        std::string conninfo;
        std::mutex conn_mutex;
        PGconn* conn;

        // -----------------------------------------------------------------------
        // INTERNAL USE ONLY
        // -----------------------------------------------------------------------

        PGresult* _db_exec(const char* query) {
            printf("[DB_EXEC] %s\n", query);
            return PQexec(conn, query);
        }

        void _describe_result_status(ExecStatusType status) {
            fprintf(stderr, "[DB_RES_STATUS] %s\n", PQresStatus(status));
        }

        void _print_conn_error(const char* fn) {
            fprintf(stderr, "[DB_ERROR] %s: %s\n", fn, PQerrorMessage(conn));
        }

        ExecStatusType _check_status(PGresult* res,
            const char* fn = "",
            const ExecStatusType status = PGRES_COMMAND_OK) {

            ExecStatusType ret = PQresultStatus(res);
            if (ret != status)
            {
                _describe_result_status(ret);
                _print_conn_error(fn);
            }
            return ret;
        }

        // char* _escape_values_query(const std::string& str) {
        //     printf("[DB_ESCAPE_VALUES] '%s'\n", str.c_str());
        // }

        // -----------------------------------------------------------------------
        // INSTANCE MANIPULATION
        // -----------------------------------------------------------------------

        ConnStatusType init(const std::string& _conninfo) {
            printf("[DB] Initializing...\n");

            std::lock_guard<std::mutex> lk(conn_mutex);
            conninfo = _conninfo;

            printf("[DB] Connecting to database with param: %s\n", conninfo.c_str());
            conn = PQconnectdb(conninfo.c_str());

            ConnStatusType status = PQstatus(conn);

            if (status != CONNECTION_OK)
            {
                fprintf(stderr, "[DB_ERROR] Can't connect to database\n");
                conninfo = "";
                PQfinish(conn);
                conn = nullptr;
            }
            else printf("[DB] Database connected: %s\n", PQdb(conn));

            if (!PQisthreadsafe())
            {
                fprintf(stderr, "[DB_WARN] Database isn't thread safe!\n");
            }

            return status;
        }

        int cancel() {
            if (!conn) return 0;
            std::lock_guard<std::mutex> lk(conn_mutex);
            PGcancel* cobj = PQgetCancel(conn);

            char err[256];
            int ret = PQcancel(cobj, err, ERRBUFSIZE);

            if (strlen(err) > 0U)
            {
                fprintf(stderr, "[DB_ERROR] Cancel error: '%s'\n", err);
            }

            PQfreeCancel(cobj);
            cobj = nullptr;

            return ret;
        }

        void shutdown() {
            printf("[DB] Shutting down...\n");
            if (conn)
            {
                if (cancel())
                {
                    printf("[DB] Latest query cancelled\n");
                }
                else printf("[DB] No query cancelled\n");

                std::lock_guard<std::mutex> lk(conn_mutex);

                PQfinish(conn);
                conn = nullptr;
            }
            else printf("[DB] No connection\n");
        }

        const std::string get_connect_param() noexcept(true) {
            return conninfo;
        }

        ExecStatusType finish_res(PGresult* res, ExecStatusType status) {
            PQclear(res);
            res = nullptr;
            return status;
        }

        bool valid_name(const std::string& str) {
            printf("[DB_VALIDATE_NAME] '%s'\n", str.c_str());
            if (std::regex_match(str, std::regex("^[0-9a-zA-Z_-]{1,100}$"))) return true;
            else return false;
        }

        // -----------------------------------------------------------------------
        // TABLE MANIPULATION
        // -----------------------------------------------------------------------

        ExecStatusType create_table_playlist(const dpp::snowflake& user_id) {
            if (!user_id) return (ExecStatusType)-1;

            std::string query("CREATE TABLE IF NOT EXISTS \"");
            query += std::to_string(user_id) + "_playlist\" ( raw JSON NOT NULL, "\
                "name VARCHAR(100) UNIQUE PRIMARY KEY NOT NULL, ts TIMESTAMP DEFAULT CURRENT_TIMESTAMP );";

            std::lock_guard<std::mutex> lk(conn_mutex);
            PGresult* res = _db_exec(query.c_str());

            ExecStatusType status = _check_status(res, "create_table_playlist");

            return finish_res(res, status);
        }

        std::pair<PGresult*, ExecStatusType>
            get_all_user_playlist(const dpp::snowflake& user_id, const get_user_playlist_type type) {

            if (!user_id) return std::make_pair(nullptr, (ExecStatusType)-1);

            std::string query("SELECT ");

            switch (type)
            {
            case gup_all: query += "*"; break;
            case gup_name_only: query += "\"name\""; break;
            case gup_raw_only: query += "\"raw\""; break;
            case gup_ts_only: query += "\"ts\""; break;
            default: return std::make_pair(nullptr, (ExecStatusType)-2);
            }

            query += std::string(" FROM \"") + std::to_string(user_id) + "_playlist\";";

            std::lock_guard<std::mutex> lk(conn_mutex);
            PGresult* res = _db_exec(query.c_str());

            ExecStatusType status = _check_status(res, "get_all_user_playlist", PGRES_TUPLES_OK);

            return std::make_pair(res, status);
        }

        std::pair<PGresult*, ExecStatusType>
            get_user_playlist(const dpp::snowflake& user_id,
                const std::string& name,
                const get_user_playlist_type type) {

            if (!user_id) return std::make_pair(nullptr, (ExecStatusType)-1);

            std::string query("SELECT ");

            switch (type)
            {
            case gup_all: query += "*"; break;
            case gup_name_only: query += "\"name\""; break;
            case gup_raw_only: query += "\"raw\""; break;
            case gup_ts_only: query += "\"ts\""; break;
            default: return std::make_pair(nullptr, (ExecStatusType)-2);
            }

            query += std::string(" FROM \"")
                + std::to_string(user_id)
                + "_playlist\" WHERE \"name\" = '"
                + name
                + "';";

            std::lock_guard<std::mutex> lk(conn_mutex);
            PGresult* res = _db_exec(query.c_str());

            ExecStatusType status = _check_status(res, "get_user_playlist", PGRES_TUPLES_OK);

            return std::make_pair(res, status);
        }

        std::pair<PGresult*, ExecStatusType>
            update_user_playlist(const dpp::snowflake& user_id, const std::string& name, const nlohmann::json& data) {

            return std::make_pair(nullptr, (ExecStatusType)-1);
            // if (!user_id) return std::make_pair(nullptr, (ExecStatusType)-1);

            // std::string data_str = data.dump();

            // std::string query("INSERT INTO \"");
            // query += std::to_string(user_id) + "_playlist\" (raw, name) VALUES (" + "" + ", '" + name + "');";

            // std::lock_guard<std::mutex> lk(conn_mutex);
            // PGresult* res = _db_exec(query.c_str());

            // ExecStatusType status = _check_status(res, "update_user_playlist", PGRES_COMMAND_OK);

            // return std::make_pair(res, status);
        }
    }
}
