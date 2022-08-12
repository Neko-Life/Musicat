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

        void _describe_result_status(ExecStatusType status, FILE* stream = stderr) {
            fprintf(stream, "[DB_RES_STATUS] %s\n", PQresStatus(status));
        }

        void _print_conn_error(const char* func_n) {
            fprintf(stderr, (std::string("[DB_ERROR] ") + func_n + ": %s\n").c_str(), PQerrorMessage(conn));
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
                fprintf(stderr, "[DB_WARN] Database isn't thread safe!");
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
            printf("[DV_VALIDATE_NAME] '%s'\n", str.c_str());
            if (std::regex_match(str, std::regex("^[0-9a-zA-Z_-]{1,100}$"))) return true;
            else return false;
        }

        // -----------------------------------------------------------------------
        // TABLE MANIPULATION
        // -----------------------------------------------------------------------

        ExecStatusType create_table_playlist(const dpp::snowflake& user_id) {
            if (!user_id) return (ExecStatusType)-1;

            std::string query("CREATE TABLE IF NOT EXISTS \"");
            query += std::to_string(user_id) + "_playlist\" ( raw JSON NOT NULL, name VARCHAR(100) UNIQUE PRIMARY KEY NOT NULL, ts TIMESTAMP DEFAULT CURRENT_TIMESTAMP );";

            std::lock_guard<std::mutex> lk(conn_mutex);
            PGresult* res = _db_exec(query.c_str());

            ExecStatusType status = PQresultStatus(res);
            if (status != PGRES_COMMAND_OK)
            {
                _describe_result_status(status);
                _print_conn_error("create_table_playlist");
            }

            return finish_res(res, status);
        }

        std::pair<PGresult*, ExecStatusType>
            get_all_user_playlist(const dpp::snowflake& user_id) {

            if (!user_id) return std::make_pair(nullptr, (ExecStatusType)-1);

            std::string query("SELECT * FROM \"");
            query += std::to_string(user_id) + "_playlist\";";

            std::lock_guard<std::mutex> lk(conn_mutex);
            PGresult* res = _db_exec(query.c_str());

            ExecStatusType status = PQresultStatus(res);
            if (status != PGRES_TUPLES_OK)
            {
                _describe_result_status(status);
                _print_conn_error("get_all_user_playlist");
            }

            return std::make_pair(res, status);
        }

        std::pair<PGresult*, ExecStatusType>
            update_user_playlist(const dpp::snowflake& user_id, const std::string& name, const nlohmann::json& data) {
            return std::make_pair(nullptr, (ExecStatusType)-1);
            // if (!user_id) return std::make_pair(nullptr, (ExecStatusType)-1);
            // if (!valid_name(name)) return std::make_pair(nullptr, (ExecStatusType)-1);

            // std::string data_str = data.dump();

            // std::string query("INSERT INTO \"");
            // query += std::to_string(user_id) + "_playlist\" (raw, name) VALUES (" + "" + ", '" + name + "');";

            // std::lock_guard<std::mutex> lk(conn_mutex);
            // PGresult* res = _db_exec(query.c_str());

            // ExecStatusType status = PQresultStatus(res);
            // if (status != PGRES_COMMAND_OK)
            // {
            //     _describe_result_status(status);
            //     _print_conn_error("get_all_user_playlist");
            // }

            // return std::make_pair(res, status);
        }
    }
}
