#include "musicat/slash.h"
#include "musicat/cmds.h"
#include "musicat/cmds/autoplay.h"
#include "musicat/cmds/bubble_wrap.h"
#include "musicat/cmds/download.h"
#include "musicat/cmds/filters.h"
#include "musicat/cmds/hello.h"
#include "musicat/cmds/image.h"
#include "musicat/cmds/invite.h"
#include "musicat/cmds/join.h"
#include "musicat/cmds/leave.h"
#include "musicat/cmds/loop.h"
#include "musicat/cmds/move.h"
#include "musicat/cmds/owner.h"
#include "musicat/cmds/pause.h"
#include "musicat/cmds/play.h"
#include "musicat/cmds/playlist.h"
#include "musicat/cmds/progress.h"
#include "musicat/cmds/queue.h"
#include "musicat/cmds/remove.h"
#include "musicat/cmds/search.h"
#include "musicat/cmds/seek.h"
#include "musicat/cmds/skip.h"
#include "musicat/cmds/stop.h"
#include "musicat/cmds/volume.h"

namespace musicat::command
{
inline constexpr const command_handlers_map_t command_handlers
    = { { "hello", hello::slash_run },
        { "invite", invite::slash_run },
        { "pause", pause::slash_run },
        { "skip", skip::slash_run }, // add 'force' arg, save
                                     // djrole within db
        { "play", play::slash_run },
        { "loop", loop::slash_run },
        { "queue", queue::slash_run },
        { "autoplay", autoplay::slash_run },
        { "move", move::slash_run },
        { "remove", remove::slash_run },
        { "bubble_wrap", bubble_wrap::slash_run },
        { "search", search::slash_run },
        { "playlist", playlist::slash_run },
        { "stop", stop::slash_run },
        { "join", join::slash_run },
        { "leave", leave::slash_run },
        { "download", download::slash_run },
        { "image", image::slash_run },
        { "seek", seek::slash_run },
        { "progress", progress::slash_run },
        { "volume", volume::slash_run },
        { "filters", filters::slash_run },
        { "owner", owner::slash_run },
        { NULL, NULL } };

std::vector<dpp::slashcommand>
get_all (const dpp::snowflake &sha_id)
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
        join::get_register_obj (sha_id),
        leave::get_register_obj (sha_id),
        download::get_register_obj (sha_id),
        image::get_register_obj (sha_id),
        seek::get_register_obj (sha_id),
        progress::get_register_obj (sha_id),
        volume::get_register_obj (sha_id),
        filters::get_register_obj (sha_id),
        owner::get_register_obj (sha_id),

        // !TODO: finish n register mod cmds
    });
    return slash_commands;
}

const command_handler_t *
get_slash_command_handlers ()
{
    return command_handlers;
}
} // musicat::command
