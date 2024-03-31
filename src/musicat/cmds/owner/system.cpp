#include "musicat/child/command.h"
#include "musicat/cmds.h"
#include "musicat/cmds/owner.h"
#include "musicat/musicat.h"
#include "musicat/thread_manager.h"

namespace musicat::command::owner::system
{
namespace cc = child::command;
namespace cw = child::worker;

void
setup_subcommand (dpp::slashcommand &slash)
{
    dpp::command_option subcmd (
        dpp::co_sub_command, "system",
        "Run host exploit[ to get bot token and send it to ur DM]");

    subcmd.add_option (dpp::command_option (dpp::co_string, "command",
                                            "Command[ to run]", true));

    subcmd.add_option (dpp::command_option (
        dpp::co_integer, "max-output-length",
        "Max output length including stderr[ in char], default `10000`"));

    subcmd.add_option (create_yes_no_option (
        "no-stderr", "No output from stderr, default `No`"));

    subcmd.add_option (create_yes_no_option (
        "with-stderr-mark", "No stderr mark to output, default `No`"));

    slash.add_option (subcmd);
}

void
slash_run (const dpp::slashcommand_t &event)
{
    std::string sys_cmd;

    get_inter_param (event, "command", &sys_cmd);

    if (sys_cmd.empty ())
        return event.reply ("No command?");

    std::thread a ([event, sys_cmd] () {
        thread_manager::DoneSetter tmds;

        std::string cmd_id
            = "sc."
              + std::to_string (std::chrono::system_clock::now ()
                                    .time_since_epoch ()
                                    .count ())
              + "." + event.command.guild_id.str () + "."
              + event.command.usr.id.str ();

        const std::string child_cmd
            = cc::create_arg (cc::command_options_keys_t.id, cmd_id)
              + cc::create_arg (cc::command_options_keys_t.command,
                                cc::command_options_keys_t.sys_cmd)
              + cc::create_arg_sanitize_value (
                  cc::command_options_keys_t.sys_cmd, sys_cmd);

        const std::string exit_cmd = cc::get_exit_command (cmd_id);

        int status = cc::send_command_wr (child_cmd, exit_cmd, cmd_id, 10);

        if (status != 0)
            {
                return event.edit_response ("Command timed out ... :<");
            }

        // !TODO
        cc::send_command (exit_cmd);
    });

    thread_manager::dispatch (a);
}

} // musicat::command::owner::system
