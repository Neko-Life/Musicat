#include <libpq-fe.h>
#include "musicat/cmds.h"
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
                            "Playlist name, must match validation regex /[0-9a-zA-Z_-]{1,100}/",
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
                    PGresult* res = database::get_all_user_playlist(event.command.usr.id);
                    database::finish_res(res);
                    res = nullptr;

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
                const std::string cmd = event.command.get_command_interaction().options.at(0).name;
                if (cmd == "save") save::slash_run(event, player_manager);
                else if (cmd == "load") load::slash_run(event, player_manager);
                else
                {
                    fprintf(stderr, "[ERROR] !!! No sub-command '%s' for 'playlist' command !!!\n", cmd.c_str());
                    event.reply("I don't have that sub-command, how'd you get that?? Please report this to my developer");
                }
            }
        }
    }
}
