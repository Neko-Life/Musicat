#include "musicat/db.h"
#include "musicat/musicat.h"
#include "musicat/player.h"
#include "nlohmann/json.hpp"
#include <cstdio>
#include <dpp/dpp.h>
#include <dpp/nlohmann/json_fwd.hpp>
#include <dpp/snowflake.h>
#include <libpq-fe.h>
#include <mutex>
#include <regex>
#include <string.h>
#include <string>
#include <vector>

namespace musicat
{
namespace database
{
std::string conninfo;
std::mutex conn_mutex;
PGconn *conn;

// -----------------------------------------------------------------------
// INTERNAL USE ONLY
// -----------------------------------------------------------------------

// STATES

bool _table_playlists_exist;
bool _table_guilds_current_queue;
bool _table_guilds_player_config_exist;

PGresult *
_db_exec (const char *query, bool debug = false)
{
    if (debug)
        fprintf (stderr, "[DB_EXEC] %s\n", query);

    return PQexec (conn, query);
}

void
_describe_result_status (ExecStatusType status)
{
    fprintf (stderr, "[DB_RES_STATUS] %s\n", PQresStatus (status));
}

void
_print_conn_error (const char *fn)
{
    fprintf (stderr, "[DB_ERROR] %s: %s\n", fn, PQerrorMessage (conn));
}

ExecStatusType
_check_status (PGresult *res, const char *fn = "",
               const ExecStatusType status = PGRES_COMMAND_OK)
{

    ExecStatusType ret = PQresultStatus (res);

    if (ret != status)
        {
            _describe_result_status (ret);
            _print_conn_error (fn);
        }

    return ret;
}

const std::string
_escape_values_query (const std::string &str)
{
    if (get_debug_state ())
        fprintf (stderr, "[DB_ESCAPE_VALUES] %ld\n", str.length ());

    char *res = PQescapeLiteral (conn, str.c_str (), str.length ());

    if (res == NULL)
        {
            _print_conn_error ("_escape_values_query");
        }

    const std::string ret (res == NULL ? "" : res);

    PQfreemem (res);
    res = NULL;
    return ret;
}

ExecStatusType
_create_table (const char *query, const char *fn = "msg")
{
    std::lock_guard<std::mutex> lk (conn_mutex);
    PGresult *res = _db_exec (query);

    ExecStatusType status = _check_status (res, fn);

    return finish_res (res, status);
}

std::string
_stringify_loop_mode (const player::loop_mode_t loop_mode)
{
    std::string tp = "-1";
    switch (loop_mode)
        {
        case player::loop_mode_t::l_none:
            tp = "0";
            break;
        case player::loop_mode_t::l_song:
            tp = "1";
            break;
        case player::loop_mode_t::l_queue:
            tp = "2";
            break;
        case player::loop_mode_t::l_song_queue:
            tp = "3";
            break;
        default:
            fprintf (stderr,
                     "[DB_ERROR] Unexpected loop mode to stringify: %d\n",
                     loop_mode);
        }
    return tp;
}

player::loop_mode_t
_parse_loop_mode (const char *loop_mode)
{
    player::loop_mode_t ret = player::loop_mode_t::l_none;
    int tp = atoi (loop_mode);
    switch (tp)
        {
        case 0:
            break;
        case 1:
            ret = player::loop_mode_t::l_song;
            break;
        case 2:
            ret = player::loop_mode_t::l_queue;
            break;
        case 3:
            ret = player::loop_mode_t::l_song_queue;
            break;
        default:
            fprintf (stderr, "[DB_ERROR] Unexpected loop mode to parse: %d\n",
                     tp);
        }
    return ret;
}

// -----------------------------------------------------------------------
// INSTANCE MANIPULATION
// -----------------------------------------------------------------------

ConnStatusType
init (const std::string &_conninfo)
{
    bool debug = get_debug_state ();
    if (debug)
        fprintf (stderr, "[DB] Initializing...\n");

    std::lock_guard<std::mutex> lk (conn_mutex);
    conninfo = _conninfo;

    if (debug)
        fprintf (stderr, "[DB] Connecting to database with param: %s\n",
                 conninfo.c_str ());

    conn = PQconnectdb (conninfo.c_str ());

    ConnStatusType status = PQstatus (conn);

    if (status != CONNECTION_OK)
        {
            fprintf (stderr, "[DB_ERROR] Can't connect to database\n");
            conninfo = "";
            PQfinish (conn);
            conn = nullptr;
        }
    else
        {
            fprintf (stderr, "[DB] Database connected: %s\n", PQdb (conn));

            _table_playlists_exist = false;
            _table_guilds_current_queue = false;
            _table_guilds_player_config_exist = false;
        }

    if (!PQisthreadsafe ())
        {
            fprintf (stderr, "[DB_WARN] Database isn't thread safe!\n");
        }

    return status;
}

ConnStatusType
reconnect (bool force, const std::string &_conninfo)
{
    bool should_reconnect = conn == nullptr || force;

    if (!should_reconnect)
        {
            // check connection status
            std::lock_guard<std::mutex> lk (conn_mutex);
            ConnStatusType status = PQstatus (conn);

            if (status == CONNECTION_OK)
                goto reconnect_ok;

            fprintf (stderr, "[DB ERROR] Connection BAD. Reconnecting...\n");
        }

    if (!_conninfo.empty ())
        conninfo = _conninfo;

    shutdown ();

    return init (conninfo);

reconnect_ok:
    return CONNECTION_OK;
}

int
cancel ()
{
    if (!conn)
        return 0;

    std::lock_guard<std::mutex> lk (conn_mutex);
    PGcancel *cobj = PQgetCancel (conn);

    char err[256];
    memset (err, '\0', sizeof (err));
    int ret = PQcancel (cobj, err, ERRBUFSIZE);

    if (strlen (err) > 0UL)
        {
            fprintf (stderr, "[DB_ERROR] Cancel error: '%s'\n", err);
        }

    PQfreeCancel (cobj);
    cobj = nullptr;

    return ret;
}

void
shutdown ()
{
    bool debug = get_debug_state ();

    if (debug)
        fprintf (stderr, "[DB] Shutting down...\n");

    if (!conn)
        {
            if (debug)
                fprintf (stderr, "[DB] No connection\n");

            return;
        }

    if (cancel ())
        {
            if (debug)
                fprintf (stderr, "[DB] Latest query cancelled\n");
        }
    else if (debug)
        fprintf (stderr, "[DB] No query cancelled\n");

    std::lock_guard<std::mutex> lk (conn_mutex);

    PQfinish (conn);
    conn = nullptr;
}

const std::string
get_connect_param () noexcept (true)
{
    return conninfo;
}

ExecStatusType
finish_res (PGresult *res, ExecStatusType status)
{
    PQclear (res);
    res = nullptr;
    return status;
}

bool
valid_name (const std::string &str)
{
    if (get_debug_state ())
        fprintf (stderr, "[DB_VALIDATE_NAME] '%s'\n", str.c_str ());

    if (std::regex_match (str, std::regex ("^[0-9a-zA-Z_ -]{1,100}$")))
        return true;
    else
        return false;
}

nlohmann::json
convert_playlist_to_json (const std::deque<player::MCTrack> &playlist)
{
    nlohmann::json jso;

    for (auto &t : playlist)
        {
            nlohmann::json r = t.raw;
            r["filename"] = t.filename;
            r["raw_info"] = t.info.raw;
            jso.push_back (r);
        }

    return jso;
}

// -----------------------------------------------------------------------
// TABLE MANIPULATION
// -----------------------------------------------------------------------

ExecStatusType
create_table_playlists ()
{
    if (_table_playlists_exist)
        return PGRES_COMMAND_OK;

    static const char query[]
        = "CREATE TABLE IF NOT EXISTS "
          "\"playlists\" ( \"raw\" JSON NOT NULL, "
          // user_id
          "\"uid\" VARCHAR(24) NOT NULL, "
          "\"name\" VARCHAR(100) NOT NULL, "
          "\"ts\" TIMESTAMPTZ DEFAULT CURRENT_TIMESTAMP NOT NULL, "
          "\"uts\" TIMESTAMPTZ DEFAULT CURRENT_TIMESTAMP NOT NULL );";

    ExecStatusType status = _create_table (query, "create_table_playlists");

    if (status == PGRES_COMMAND_OK)
        {
            _table_playlists_exist = true;
        }

    return status;
}

ExecStatusType
create_table_guilds_current_queue ()
{
    if (_table_guilds_current_queue)
        return PGRES_COMMAND_OK;

    static const char query[]
        = "CREATE TABLE IF NOT EXISTS \""
          "guilds_current_queue\" ( \"raw\" JSON NOT NULL, "
          // guild_id
          "\"gid\" VARCHAR(24) UNIQUE PRIMARY KEY NOT NULL, "
          "\"ts\" TIMESTAMPTZ DEFAULT CURRENT_TIMESTAMP NOT NULL, "
          "\"uts\" TIMESTAMPTZ DEFAULT CURRENT_TIMESTAMP NOT NULL );";

    ExecStatusType status
        = _create_table (query, "create_table_guilds_current_queue");

    if (status == PGRES_COMMAND_OK)
        {
            _table_guilds_current_queue = true;
        }

    return status;
}

ExecStatusType
create_table_guilds_player_config ()
{
    if (_table_guilds_player_config_exist)
        return PGRES_COMMAND_OK;

    static const char query[]
        = "CREATE TABLE IF NOT EXISTS \""
          "guilds_player_config\" ( \"autoplay_state\" BOOL DEFAULT FALSE, "
          "\"autoplay_threshold\" INT DEFAULT 0 "
          "CHECK (\"autoplay_threshold\" >= 0 AND \"autoplay_threshold\" <= "
          "10000), "
          "\"loop_mode\" SMALLINT DEFAULT 0 "
          "CHECK (\"loop_mode\" >= 0 AND \"loop_mode\" <= 10), "
          // guild_id
          "\"gid\" VARCHAR(24) UNIQUE PRIMARY KEY NOT NULL, "
          "\"ts\" TIMESTAMPTZ DEFAULT CURRENT_TIMESTAMP NOT NULL, "
          "\"uts\" TIMESTAMPTZ DEFAULT CURRENT_TIMESTAMP NOT NULL );";

    ExecStatusType status
        = _create_table (query, "create_table_guilds_player_config");

    if (status == PGRES_COMMAND_OK)
        {
            _table_guilds_player_config_exist = true;
        }

    return status;
}

std::pair<PGresult *, ExecStatusType>
get_all_user_playlist (const dpp::snowflake &user_id,
                       const get_user_playlist_type type)
{

    if (!user_id)
        return std::make_pair (nullptr, (ExecStatusType)-1);

    std::string query ("SELECT ");

    switch (type)
        {
        case gup_all:
            query += "*";
            break;
        case gup_name_only:
            query += "\"name\"";
            break;
        case gup_raw_only:
            query += "\"raw\"";
            break;
        case gup_ts_only:
            query += "\"ts\"";
            break;
        default:
            return std::make_pair (nullptr, (ExecStatusType)-2);
        }

    query += " FROM \"playlists\" WHERE \"uid\" = '" + std::to_string (user_id)
             + "';";

    std::lock_guard<std::mutex> lk (conn_mutex);
    PGresult *res = _db_exec (query.c_str ());

    ExecStatusType status
        = _check_status (res, "get_all_user_playlists", PGRES_TUPLES_OK);

    return std::make_pair (res, status);
}

std::pair<PGresult *, ExecStatusType>
get_user_playlist (const dpp::snowflake &user_id, const std::string &name,
                   const get_user_playlist_type type)
{

    if (!user_id)
        return std::make_pair (nullptr, (ExecStatusType)-1);

    if (!valid_name (name))
        {
            return std::make_pair (nullptr, (ExecStatusType)-3);
        }

    std::string query ("SELECT ");

    switch (type)
        {
        case gup_all:
            query += "*";
            break;
        case gup_name_only:
            query += "\"name\"";
            break;
        case gup_raw_only:
            query += "\"raw\"";
            break;
        case gup_ts_only:
            query += "\"ts\"";
            break;
        default:
            return std::make_pair (nullptr, (ExecStatusType)-2);
        }

    query += " FROM \"playlists\" WHERE \"uid\" = '" + std::to_string (user_id)
             + "' AND \"name\" = '" + name + "';";

    std::lock_guard<std::mutex> lk (conn_mutex);
    PGresult *res = _db_exec (query.c_str ());

    ExecStatusType status
        = _check_status (res, "get_user_playlists", PGRES_TUPLES_OK);

    return std::make_pair (res, status);
}

std::pair<std::deque<player::MCTrack>, int>
get_playlist_from_PGresult (PGresult *res)
{
    std::deque<player::MCTrack> ret = {};
    if (PQgetisnull (res, 0, 0))
        return std::make_pair (ret, -2);

    nlohmann::json jso = nlohmann::json::parse (PQgetvalue (res, 0, 0));

    if (jso.is_null () || !jso.is_array () || jso.empty ())
        return std::make_pair (ret, -1);

    for (auto j = jso.begin (); j != jso.end (); j++)
        {
            if (j->is_null ())
                continue;

            player::MCTrack t;
            t.raw = *j;
            t.filename = j->at ("filename").get<std::string> ();
            t.info.raw = j->at ("raw_info");

            ret.push_back (t);
        }

    return std::make_pair (ret, 0);
}

ExecStatusType
update_user_playlist (const dpp::snowflake &user_id, const std::string &name,
                      const std::deque<player::MCTrack> &playlist)
{

    if (!user_id)
        return (ExecStatusType)-1;

    const std::string str_user_id = std::to_string (user_id);

    nlohmann::json jso = convert_playlist_to_json (playlist);

    const std::string values = _escape_values_query (jso.dump ());

    std::lock_guard<std::mutex> lk (conn_mutex);

    std::string query_update (
        "UPDATE \"playlists\" SET "
        "\"raw\" = "
        + values + ", \"uts\" = CURRENT_TIMESTAMP WHERE \"uid\" = '"
        + str_user_id + "' AND \"name\" = '" + name + "' RETURNING \"name\";");

    PGresult *res = _db_exec (query_update.c_str ());

    ExecStatusType status
        = _check_status (res, "update_user_playlist", PGRES_TUPLES_OK);

    bool not_updated = false;
    if (PQgetisnull (res, 0, 0))
        not_updated = true;
    else
        status = PGRES_COMMAND_OK;

    if (not_updated)
        {
            finish_res (res, status);

            std::string query_insert (
                "INSERT INTO \"playlists\" "
                "(\"uid\", \"raw\", \"name\") VALUES ('");

            query_insert
                += str_user_id + "', " + values + ", '" + name + "');";

            res = _db_exec (query_insert.c_str ());

            status = _check_status (res, "insert_user_playlist");
        }

    return finish_res (res, status);
}

ExecStatusType
delete_user_playlist (const dpp::snowflake &user_id, const std::string &name)
{

    if (!user_id)
        return (ExecStatusType)-1;

    std::string query ("DELETE FROM \"");
    query += "playlists\" "
             "WHERE \"uid\" = '"
             + std::to_string (user_id) + "' AND \"name\" = '" + name
             + "' RETURNING \"name\" ;";

    std::lock_guard<std::mutex> lk (conn_mutex);
    PGresult *res = _db_exec (query.c_str ());

    ExecStatusType status = PGRES_FATAL_ERROR;

    if (!PQgetisnull (res, 0, 0))
        status = _check_status (res, "delete_user_playlist", PGRES_TUPLES_OK);
    else
        _check_status (res, "delete_user_playlist", PGRES_TUPLES_OK);

    return finish_res (res, status);
}

ExecStatusType
update_guild_current_queue (const dpp::snowflake &guild_id,
                            const std::deque<player::MCTrack> &playlist)
{
    if (!guild_id)
        return (ExecStatusType)-1;
    if (!playlist.size ())
        return (ExecStatusType)-2;

    if (create_table_guilds_current_queue () != PGRES_COMMAND_OK)
        {
            fprintf (stderr,
                     "[DB_ERROR] Failed to create table guilds_current_queue, "
                     "can't update guild current queue\n");
            return (ExecStatusType)-3;
        }

    nlohmann::json jso = convert_playlist_to_json (playlist);

    const std::string values = _escape_values_query (jso.dump ());

    std::string query ("INSERT INTO \"guilds_current_queue\" ");
    query += "( \"raw\", \"gid\") VALUES (" + values + ", '"
             + std::to_string (guild_id)
             + "') ON CONFLICT (\"gid\") DO UPDATE SET \"raw\" = " + values
             + ", \"uts\" = CURRENT_TIMESTAMP;";

    std::lock_guard<std::mutex> lk (conn_mutex);
    PGresult *res = _db_exec (query.c_str ());

    ExecStatusType status = _check_status (res, "update_guild_current_queue");

    return finish_res (res, status);
}

std::pair<PGresult *, ExecStatusType>
get_guild_current_queue (const dpp::snowflake &guild_id)
{
    if (!guild_id)
        return std::make_pair (nullptr, (ExecStatusType)-1);

    std::string query (
        "SELECT \"raw\" FROM \"guilds_current_queue\" WHERE \"gid\" = '");
    query += std::to_string (guild_id) + "';";

    std::lock_guard<std::mutex> lk (conn_mutex);
    PGresult *res = _db_exec (query.c_str ());

    ExecStatusType status
        = _check_status (res, "get_guild_current_queue", PGRES_TUPLES_OK);

    return std::make_pair (res, status);
}

ExecStatusType
delete_guild_current_queue (const dpp::snowflake &guild_id)
{
    if (!guild_id)
        return (ExecStatusType)-1;

    std::string query ("DELETE FROM \"");
    query += "guilds_current_queue\" "
             "WHERE \"gid\" = '"
             + std::to_string (guild_id) + "' RETURNING \"gid\" ;";

    std::lock_guard<std::mutex> lk (conn_mutex);
    PGresult *res = _db_exec (query.c_str ());

    ExecStatusType status = PGRES_FATAL_ERROR;

    if (!PQgetisnull (res, 0, 0))
        status = _check_status (res, "delete_guild_current_queue",
                                PGRES_TUPLES_OK);
    else
        _check_status (res, "delete_guild_current_queue", PGRES_TUPLES_OK);

    return finish_res (res, status);
}

ExecStatusType
update_guild_player_config (const dpp::snowflake &guild_id,
                            const bool *autoplay_state,
                            const int *autoplay_threshold,
                            const player::loop_mode_t *loop_mode)
{

    if (!guild_id)
        return (ExecStatusType)-1;

    if (create_table_guilds_player_config () != PGRES_COMMAND_OK)
        {
            fprintf (stderr,
                     "[DB_ERROR] Failed to create table guilds_player_config, "
                     "can't update guild player config\n");
            return (ExecStatusType)-3;
        }

    if (!autoplay_state && !autoplay_threshold && !loop_mode)
        {
            // nothing to update
            return (ExecStatusType)-2;
        }

    std::vector<std::string> values, names;
    values.reserve (3);
    names.reserve (3);

    if (autoplay_state)
        {
            names.push_back ("\"autoplay_state\"");
            values.push_back (*autoplay_state ? "TRUE" : "FALSE");
        }

    if (autoplay_threshold)
        {
            names.push_back ("\"autoplay_threshold\"");
            values.push_back (std::to_string (*autoplay_threshold));
        }

    if (loop_mode)
        {
            names.push_back ("\"loop_mode\"");
            values.push_back (_stringify_loop_mode (*loop_mode));
        }

    std::string query ("INSERT INTO \"guilds_player_config\" ");
    query += "( \"gid\", ";

    for (auto i = names.begin (); i != names.end (); i++)
        {
            query += *i;
            if ((i + 1) != names.end ())
                query += ", ";
        }

    query += " ) VALUES ( '" + std::to_string (guild_id) + "', ";

    for (auto i = values.begin (); i != values.end (); i++)
        {
            query += *i;
            if ((i + 1) != values.end ())
                query += ", ";
        }

    query += " ) ON CONFLICT (\"gid\") DO UPDATE SET ";

    for (size_t i = 0; i < names.size (); i++)
        {
            query += names[i] + " = " + values[i] + ", ";
        }

    query += "\"uts\" = CURRENT_TIMESTAMP;";

    std::lock_guard<std::mutex> lk (conn_mutex);
    PGresult *res = _db_exec (query.c_str (), get_debug_state ());

    ExecStatusType status = _check_status (res, "update_guild_player_config");

    return finish_res (res, status);
}

std::pair<PGresult *, ExecStatusType>
get_guild_player_config (const dpp::snowflake &guild_id)
{
    if (!guild_id)
        return std::make_pair (nullptr, (ExecStatusType)-1);

    std::string query (
        "SELECT \"autoplay_state\", \"autoplay_threshold\", \"loop_mode\" "
        "FROM \"guilds_player_config\" WHERE \"gid\" = '");
    query += std::to_string (guild_id) + "';";

    std::lock_guard<std::mutex> lk (conn_mutex);
    PGresult *res = _db_exec (query.c_str ());

    ExecStatusType status
        = _check_status (res, "get_guild_player_config", PGRES_TUPLES_OK);

    return std::make_pair (res, status);
}

std::pair<player_config, int>
parse_guild_player_config_PGresult (PGresult *res)
{
    player_config ret;
    ret.autoplay_state = false;
    ret.autoplay_threshold = 0;
    ret.loop_mode = player::loop_mode_t::l_none;

    bool set = false;
    const bool debug = get_debug_state ();

    if (!PQgetisnull (res, 0, 0))
        {
            const char *val = PQgetvalue (res, 0, 0);
            if (debug)
                fprintf (stderr,
                         "[DB_DEBUG] Parse player config atp_state: %s\n",
                         val);
            if (strcmp ("t", val) == 0)
                {
                    ret.autoplay_state = true;
                    set = true;
                }
        }

    if (!PQgetisnull (res, 0, 1))
        {
            const char *val = PQgetvalue (res, 0, 1);
            if (debug)
                fprintf (stderr,
                         "[DB_DEBUG] Parse player config atp_thres: %s\n",
                         val);

            ret.autoplay_threshold = atoi (val);
            set = true;
        }

    if (!PQgetisnull (res, 0, 2))
        {
            const char *val = PQgetvalue (res, 0, 2);
            if (debug)
                fprintf (stderr, "[DB_DEBUG] Parse player config l_mode: %s\n",
                         val);
            ret.loop_mode = _parse_loop_mode (val);
            set = true;
        }

    return std::make_pair (ret, set ? 0 : 1);
}

} // database
} // musicat
