#include <regex>
#include "musicat/musicat.h"
#include "musicat/slash.h"

#define PRINT_USAGE_REGISTER_SLASH printf("Usage:\n\treg <guild_id|\"g\">\n\trm-reg <guild_id|\"g\">\n")

namespace musicat {
    int _reg(dpp::cluster& client, dpp::snowflake sha_id, int argc, const char* argv[], bool rm = false, bool* running_state = nullptr) {
        if (argc == 2)
        {
            printf("Provide guild_id or \"g\" to register globally\n");
            return 0;
        }
        std::string a2 = std::string(argv[2]);
        if (a2 == "g")
        {
            printf((std::string(rm ? "Deleting" : "Registering") + " commands globally...\n").c_str());
            if (rm)
            {
                client.me.id = sha_id;
                client.global_commands_get([&client, &running_state](const dpp::confirmation_callback_t& res) {
                    if (res.is_error())
                    {
                        auto e = res.get_error();
                        fprintf(stderr, "ERROR %d: %s\n", e.code, e.message.c_str());
                        return;
                    }
                    auto val = std::get<dpp::slashcommand_map>(res.value);
                    for (const auto& i : val)
                    {
                        auto id = i.second.id;
                        printf("%ld ", id);
                        client.global_command_delete(id);
                    }
                    *running_state = false;
                });
                return 0;
            }
            auto c = command::get_all(sha_id);
            client.global_bulk_command_create(c);
            *running_state = false;
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
            printf((std::string(rm ? "Deleting" : "Registering") + " commands in %ld\n").c_str(), gid);
            if (rm)
            {
                client.me.id = sha_id;
                client.guild_commands_get(gid, [&client, gid, &running_state](const dpp::confirmation_callback_t& res) {
                    if (res.is_error())
                    {
                        auto e = res.get_error();
                        fprintf(stderr, "ERROR %d: %s\n", e.code, e.message.c_str());
                        return;
                    }
                    auto val = std::get<dpp::slashcommand_map>(res.value);
                    for (const auto& i : val)
                    {
                        auto id = i.second.id;
                        printf("%ld ", id);
                        client.guild_command_delete(id, gid);
                    }
                    *running_state = false;
                });
                return 0;
            }
            auto c = command::get_all(sha_id);
            client.guild_bulk_command_create(c, gid);
            *running_state = false;
            return 0;
        }
    }

    int cli(dpp::cluster& client, dpp::snowflake sha_id, int argc, const char* argv[], bool* running_state)
    {
        std::string a1 = std::string(argv[1]);
        // for (int i = 1; i <= argc; i++){
        //     auto a=argv[i];
        //     if (a)
        // }
        int cmd = -1;
        if (a1 == "reg") cmd = 0;
        if (a1 == "rm-reg") cmd = 1;

        if (cmd < 0)
        {
            PRINT_USAGE_REGISTER_SLASH;
            return 0;
        }

        switch (cmd)
        {
        case 0:
        case 1:
            return _reg(client, sha_id, argc, argv, cmd == 1, running_state);
        }
        return 1;
    }
}
