#include <libpq-fe.h>
#include <string>
#include "musicat/db.h"

namespace musicat {
    namespace database {
        std::string conninfo;
        PGconn* conn;

        int init(std::string _conninfo) {
            printf("[DB] Initializing...\n");
            conninfo = _conninfo;

            printf("[DB] Connecting to database with param: %s\n", conninfo.c_str());
            conn = PQconnectdb(conninfo.c_str());

            auto status = PQstatus(conn);

            if (status != CONNECTION_OK)
            {
                fprintf(stderr, "[DB] Can't connect to database\n");
                PQfinish(conn);
                conn = nullptr;
            }
            else printf("[DB] Database connected: %s\n", PQdb(conn));

            return status;
        }

        void shutdown() {
            if (conn)
            {
                PQfinish(conn);
                conn = nullptr;
            }
        }
    }
}
