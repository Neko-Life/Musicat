#include "musicat/musicat.h"
#include "musicat/slash.h"
#include <regex>

void
print_usage_register_slash ()
{
    fprintf (stderr,
             "Usage:\n\treg <guild_id|\"g\">\n\trm-reg <guild_id|\"g\">\n");
}

namespace musicat
{

void
command_create_callback (const dpp::confirmation_callback_t &e)
{
    if (e.is_error ())
        {
            std::cerr << "ERROR:" << '\n';

            dpp::error_info ev = e.get_error ();
            for (auto eve : ev.errors)
                {
                    std::cerr << eve.code << '\n';
                    std::cerr << eve.field << '\n';
                    std::cerr << eve.reason << '\n';
                    std::cerr << eve.object << '\n';
                }

            std::cerr << ev.code << '\n';
            std::cerr << ev.message << '\n';
        }

    set_running_state (false);
}

int
exec_delete_global_command (dpp::cluster &client)
{
    client.global_bulk_command_create ({}, command_create_callback);

    return 0;
}

int
exec_delete_guild_command (dpp::cluster &client, const dpp::snowflake &gid)
{
    client.guild_bulk_command_create ({}, gid, command_create_callback);

    return 0;
}

int
_reg (dpp::cluster &client, dpp::snowflake sha_id, int argc,
      const char *argv[], bool rm = false)
{
    if (argc == 2)
        {
            fprintf (stderr,
                     "Provide guild_id or \"g\" to register globally\n");
            set_running_state (false);
            return 0;
        }

    std::string a2 = std::string (argv[2]);

    if (a2 == "g")
        {
            // global commands operation
            fprintf (stderr, "%s commands globally...\n",
                     rm ? "Deleting" : "Registering");

            if (rm)
                {
                    client.me.id = sha_id;

                    return exec_delete_global_command (client);
                }

            auto c = command::get_all (sha_id);
            client.global_bulk_command_create (c, command_create_callback);
            return 0;
        }

    // guild commands operation

    if (!std::regex_match (argv[2], std::regex ("^\\d{17,20}$"),
                           std::regex_constants::match_any))
        {
            fprintf (stderr, "Provide valid guild_id\n");
            set_running_state (false);
            return 0;
        }

    int64_t gid;
    std::istringstream iss (argv[2]);
    iss >> gid;

    if (gid < 0)
        {
            fprintf (stderr, "Invalid integer, too large\n");
            set_running_state (false);
            return 0;
        }

    fprintf (
        stderr,
        (std::string (rm ? "Deleting" : "Registering") + " commands in %ld\n")
            .c_str (),
        gid);

    if (rm)
        {
            client.me.id = sha_id;

            return exec_delete_guild_command (client, gid);
        }

    auto c = command::get_all (sha_id);
    client.guild_bulk_command_create (c, gid, command_create_callback);
    return 0;
}

int
cli (dpp::cluster &client, dpp::snowflake sha_id, int argc, const char *argv[])
{
    std::string a1 = std::string (argv[1]);
    // for (int i = 1; i <= argc; i++){
    //     auto a=argv[i];
    //     if (a)
    // }
    int cmd = -1;
    if (a1 == "reg")
        cmd = 0;
    if (a1 == "rm-reg")
        cmd = 1;

    if (cmd < 0)
        {
            print_usage_register_slash ();
            return set_running_state (false);
        }

    switch (cmd)
        {
        case 0:
        case 1:
            return _reg (client, sha_id, argc, argv, cmd == 1);
        }
    return 1;
}
} // namespace musicat
