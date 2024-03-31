#include "musicat/child/command.h"
#include "musicat/child/worker.h"
#include "musicat/cmds.h"
#include "musicat/cmds/owner.h"
#include "musicat/musicat.h"

namespace musicat::command::owner::system
{
namespace cc = child::command;

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

    std::string cmd_id
        = "sys_cmd."
          + std::to_string (
              std::chrono::system_clock::now ().time_since_epoch ().count ())
          + "." + std::to_string (event.command.guild_id) + "."
          + std::to_string (event.command.usr.id);

    std::string child_cmd = cc::command_options_keys_t.command + '='
                            + cc::command_options_keys_t.sys_cmd + ';'
                            + cc::command_options_keys_t.sys_cmd + '='
                            + cc::sanitize_command_value (sys_cmd) + ';'
                            + cc::command_options_keys_t.id + '=' + cmd_id
                            + ';';

    cc::write_command (sys_cmd);

    int status = cc::wait_slave_ready (cmd_id, 10);

    const std::string exit_cmd
        = cc::command_options_keys_t.id + '=' + cmd_id + ';'
          + cc::command_options_keys_t.command + '='
          + cc::command_execute_commands_t.shutdown + ';';

    if (status == child::worker::ready_status_t.ERR_SLAVE_EXIST
        || status == child::worker::ready_status_t.TIMEOUT)
        {
            // status won't be 0 if this block is executed
            const std::string force_exit_cmd
                = exit_cmd + cc::command_options_keys_t.force + "=1;";

            cc::send_command (force_exit_cmd);

            return event.edit_response ("Command timeout :(");
        }

    if (status == 0)
        {
        }
}

} // musicat::command::owner::system
