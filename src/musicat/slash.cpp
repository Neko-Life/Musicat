#include "musicat/slash.h"
#include "musicat/player.h"
#include "musicat/cmds.h"

namespace musicat {
    namespace command {
        std::vector<dpp::slashcommand> get_all(dpp::snowflake sha_id) {
            std::vector<dpp::slashcommand> slash_commands({
                hello::get_register_obj(sha_id),
                invite::get_register_obj(sha_id),
                play::get_register_obj(sha_id),
                skip::get_register_obj(sha_id),
                pause::get_register_obj(sha_id),
                loop::get_register_obj(sha_id),
                queue::get_register_obj(sha_id),
                autoplay::get_register_obj(sha_id),
                move::get_register_obj(sha_id),
                remove::get_register_obj(sha_id),
                bubble_wrap::get_register_obj(sha_id),
                search::get_register_obj(sha_id),
                playlist::get_register_obj(sha_id),
                stop::get_register_obj(sha_id),
                });
            return slash_commands;
        }
    }
}
