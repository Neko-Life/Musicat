#include "musicat/slash.h"
#include "musicat/cmds.h"

namespace musicat
{
namespace command
{
std::vector<dpp::slashcommand>
get_all (dpp::snowflake sha_id)
{
    std::vector<dpp::slashcommand> slash_commands ({
        hello::get_register_obj (sha_id),
        invite::get_register_obj (sha_id),
        play::get_register_obj (sha_id),
        skip::get_register_obj (sha_id),
        pause::get_register_obj (sha_id),
        loop::get_register_obj (sha_id),
        queue::get_register_obj (sha_id),
        autoplay::get_register_obj (sha_id),
        move::get_register_obj (sha_id),
        remove::get_register_obj (sha_id),
        bubble_wrap::get_register_obj (sha_id),
        search::get_register_obj (sha_id),
        playlist::get_register_obj (sha_id),
        stop::get_register_obj (sha_id),
        // interactive_message::get_register_obj(sha_id),
        join::get_register_obj (sha_id),
        leave::get_register_obj (sha_id),
        download::get_register_obj (sha_id),
        image::get_register_obj (sha_id),
        seek::get_register_obj (sha_id),
        progress::get_register_obj (sha_id),
        volume::get_register_obj (sha_id),
    });
    return slash_commands;
}
} // command
} // musicat
