#include "musicat/child/command.h"
#include "musicat/child/worker.h"
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

    subcmd.add_option (
        dpp::command_option (dpp::co_integer, "with-stderr-mark",
                             "No stderr mark to output, default `No`", false));

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

        std::string child_cmd;
        cc::set_arg (child_cmd, cc::command_options_keys_t.id, cmd_id);
        cc::set_arg (child_cmd, cc::command_options_keys_t.command,
                     cc::command_options_keys_t.sys_cmd);
        cc::set_arg_sanitize_value (
            child_cmd, cc::command_options_keys_t.sys_cmd, sys_cmd);

        cc::write_command (child_cmd);

        int status = cc::wait_slave_ready (cmd_id, 10);

        std::string exit_cmd;
        cc::set_arg (exit_cmd, cc::command_options_keys_t.id, cmd_id);
        cc::set_arg (exit_cmd, cc::command_options_keys_t.command,
                     cc::command_execute_commands_t.shutdown);

        if (cw::should_bail_out_afayc (status))
            {
                // status won't be 0 if this block is executed
                const std::string force_exit_cmd
                    = exit_cmd + cc::command_options_keys_t.force + "=1;";

                cc::send_command (force_exit_cmd);

                return event.edit_response ("Command timed out ... :<");
            }
    });

    thread_manager::dispatch (a);
}

} // musicat::command::owner::system
