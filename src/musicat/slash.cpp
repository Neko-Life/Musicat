#include "musicat/slash.h"
#include "musicat/player.h"
#include "musicat/cmds.h"

namespace mpl = musicat_player;
namespace mcmd = musicat_command;

namespace musicat_slash {
    using string = std::string;
    std::vector<dpp::slashcommand> get_all(dpp::snowflake sha_id) {
        std::vector<dpp::slashcommand> slash_commands({
            mcmd::hello::get_register_obj(sha_id),
            mcmd::invite::get_register_obj(sha_id),
            mcmd::play::get_register_obj(sha_id),
            mcmd::skip::get_register_obj(sha_id),
            mcmd::pause::get_register_obj(sha_id),
            mcmd::loop::get_register_obj(sha_id),
            mcmd::queue::get_register_obj(sha_id),
            mcmd::autoplay::get_register_obj(sha_id),
            mcmd::move::get_register_obj(sha_id),
            });
        return slash_commands;
    }
}
