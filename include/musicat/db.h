#ifndef MUSICAT_DATABASE_H
#define MUSICAT_DATABASE_H

#include <string>

namespace musicat {
    namespace database {
        /**
         * @brief Initialize database, load config and connecting to server
         *
         * @param _conninfo Connection param
         * @return int Return -1 on error, 0 on success
         */
        int init(std::string _conninfo);

        /**
         * @brief Destroy database instance, must be called to free the memory (eg. before exiting program)
         *
         */
        void shutdown();
    }
}

#endif // MUSICAT_DATABASE_H
