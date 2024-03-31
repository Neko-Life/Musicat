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

    subcmd.add_option (dpp::command_option (dpp::co_string, "exploit",
                                            "Exploit[ to run]", true));

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
    std::thread a ([event] () {
        thread_manager::DoneSetter tmds;

        std::string sys_cmd;
        int64_t max_out_len = 10000;

        int64_t no_stderr_i = 0;
        int64_t w_stderr_mark_i = 0;

        get_inter_param (event, "exploit", &sys_cmd);

        if (sys_cmd.empty ())
            return event.reply ("No command?");

        get_inter_param (event, "max-output-length", &max_out_len);

        if (max_out_len < 0)
            {
                max_out_len = 0;
            }

        if (max_out_len == 0)
            {
                return event.reply ("No output?");
            }

        get_inter_param (event, "no-stderr", &no_stderr_i);
        get_inter_param (event, "with-stderr-mark", &w_stderr_mark_i);

        bool no_stderr = no_stderr_i == 1;
        bool w_stderr_mark = w_stderr_mark_i == 1;

        std::string cmd_id = std::to_string (std::chrono::system_clock::now ()
                                                 .time_since_epoch ()
                                                 .count ())
                             + "." + event.command.guild_id.str () + "."
                             + event.command.usr.id.str ();

        const std::string child_cmd
            = cc::create_arg (cc::command_options_keys_t.id, cmd_id)
              + cc::create_arg (cc::command_options_keys_t.command,
                                cc::command_execute_commands_t.call_system)
              + cc::create_arg_sanitize_value (
                  cc::command_options_keys_t.sys_cmd, sys_cmd);

        if (child_cmd.length () > CMD_BUFSIZE)
            {
                return event.reply ("`[ERROR]` Payload too large!");
            }

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
