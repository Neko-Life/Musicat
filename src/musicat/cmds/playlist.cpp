#include <libpq-fe.h>
#include "musicat/cmds.h"
#include "musicat/autocomplete.h"
#include "musicat/db.h"

namespace musicat {
    namespace command {
        namespace playlist {
            namespace save {
                namespace autocomplete {
                    void save() {}
                }

                dpp::command_option get_option_obj() {
                    return dpp::command_option(
                        dpp::co_sub_command,
                        "save",
                        "Save [current playlist]"
                    ).add_option(
                        dpp::command_option(
                            dpp::co_string,
                            "id",
                            "Playlist name, must match validation regex /^[0-9a-zA-Z_-]{1,100}$/",
                            true
                        ).set_auto_complete(true)
                    );
                }

                void slash_run(const dpp::interaction_create_t& event, player_manager_ptr player_manager) {
                    database::create_table_playlist(event.command.usr.id);
                    event.reply("Ye");
                }
            }

            namespace load {
                namespace autocomplete {
                    void id(const dpp::autocomplete_t& event, std::string param) {
                        std::pair<PGresult*, ExecStatusType>
                            res = database::get_all_user_playlist(event.command.usr.id);

                        std::vector<std::pair<std::string, std::string>> response = {};

                        if (res.second == PGRES_TUPLES_OK)
                        {
                            int rn = 0;
                            while (true)
                            {
                                if (PQgetisnull(res.first, rn, 1)) break;

                                std::string val = std::string(PQgetvalue(res.first, rn, 1));

                                response.push_back(std::make_pair(val, val));
                                rn++;
                            }
                        }

                        database::finish_res(res.first);
                        res.first = nullptr;

                        musicat::autocomplete::create_response(
                            musicat::autocomplete::filter_candidates(response, param), event);
                    }
                }

                dpp::command_option get_option_obj() {
                    return dpp::command_option(
                        dpp::co_sub_command,
                        "load",
                        "Load [saved playlist]"
                    ).add_option(
                        dpp::command_option(
                            dpp::co_string,
                            "id",
                            "Playlist name",
                            true
                        ).set_auto_complete(true)
                    );
                }

                void slash_run(const dpp::interaction_create_t& event, player_manager_ptr player_manager) {
                    std::pair<PGresult*, ExecStatusType>
                        res = database::get_all_user_playlist(event.command.usr.id);
                    database::finish_res(res.first);
                    res.first = nullptr;

                    event.reply("Aight");
                }
            }

            dpp::slashcommand get_register_obj(const dpp::snowflake sha_id) {
                return dpp::slashcommand("playlist", "Your playlist manager", sha_id)
                    .add_option(
                        save::get_option_obj()
                    ).add_option(
                        load::get_option_obj()
                    );
            }

            void slash_run(const dpp::interaction_create_t& event, player_manager_ptr player_manager) {
                auto inter = event.command.get_command_interaction();

                if (inter.options.begin() == inter.options.end())
                {
                    fprintf(stderr, "[ERROR] !!! No options for command 'playlist' !!!\n");
                    event.reply("I don't have that command, how'd you get that?? Please report this to my developer");
                    return;
                }

                const std::string cmd = inter.options.at(0).name;
                if (cmd == "save") save::slash_run(event, player_manager);
                else if (cmd == "load") load::slash_run(event, player_manager);
                else
                {
                    fprintf(stderr, "[ERROR] !!! NO SUB-COMMAND '%s' FOR COMMAND 'playlist' !!!\n", cmd.c_str());
                    event.reply("Somethin went horribly wrong.... Please quickly report this to my developer");
                }
            }
        }
    }
}
