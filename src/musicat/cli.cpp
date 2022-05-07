#include <regex>
#include "musicat/musicat.h"
#include "musicat/slash.h"

#define PRINT_USAGE_REGISTER_SLASH printf("Usage:\n  reg <guild_id|\"g\">\n")

namespace msl = musicat_slash;

namespace musicat {
    int cli(dpp::cluster& client, dpp::snowflake sha_id, int argc, const char* argv[])
    {
        string a1 = string(argv[1]);
        // for (int i = 1; i <= argc; i++){
        //     auto a=argv[i];
        //     if (a)
        // }
        int cmd = -1;
        if (a1 == "reg") cmd = 0;

        if (cmd < 0)
        {
            PRINT_USAGE_REGISTER_SLASH;
            return 0;
        }

        switch (cmd)
        {
        case 0:
            if (argc == 2)
            {
                printf("Provide guild_id or \"g\" to register globally\n");
                return 0;
            }
            string a2 = string(argv[2]);
            if (a2 == "g")
            {
                printf("Registering commands globally...\n");
                auto c = msl::get_all(sha_id);
                client.global_bulk_command_create(c);
                return 0;
            }
            else
            {
                if (!std::regex_match(argv[2], std::regex("^\\d{17,20}$"), std::regex_constants::match_any))
                {
                    printf("Provide valid guild_id\n");
                    return 0;
                }
                int64_t gid;
                std::istringstream iss(argv[2]);
                iss >> gid;
                if (gid < 0)
                {
                    printf("Invalid integer, too large\n");
                    return 0;
                }
                printf("Registering commands in %ld\n", gid);
                auto c = msl::get_all(sha_id);
                client.guild_bulk_command_create(c, gid);
                return 0;
            }
            break;
        }

        return 0;
    }
}
