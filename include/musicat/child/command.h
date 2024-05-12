#ifndef MUSICAT_CHILD_COMMAND_H
#define MUSICAT_CHILD_COMMAND_H

#include "musicat/child.h"
#include "musicat/child/worker.h"
#include "musicat/musicat.h"
#include <string>

namespace musicat::child::command
{

// update worker::execute and related routines when changing this
inline const struct
{
    const std::string create_audio_processor = "cap";
    const std::string shutdown = "shut";

    /**
     * Invoking ytdlp cmd and save the output to a json file
     *
     * Requires id, ytdlp_util_exe, ytdlp_lib_path and ytdlp_query
     * Optionally ytdlp_max_entries
     *
     * Creates a fifo based on id
     * Caller should open and read the fifo to get the cmd output
     * containing file_path of the result to open and read
     *
     * If file_path is empty then smt wrong happened
     * !TODO: result status?
     */
    const std::string call_ytdlp = "ytd";

    /**
     * Call system
     *
     * Requires sys_cmd (the command gonna be passed to system()),
     * sys_max_out_len (max output length), sys_no_stderr (hide stderr output),
     * sys_w_stderr_mark (mark stderr output) command is call_system, and ofc
     * id too
     *
     * Write to an output file and write to out fifo with file path
     *
     * Caller manage (eg. manually delete) the output file after this job
     * exited
     */
    const std::string call_system = "sys";
} command_execute_commands_t;

// update set_option impl in child/command.cpp when changing this
inline const struct
{
    const std::string command = "cmd";  // str
    const std::string file_path = "fp"; // str
    const std::string debug = "dbg";    // bool
    const std::string id = "id";        // str
    const std::string guild_id = "gid"; // str
    const std::string ready = "rdy";    // bool
    const std::string seek = "sk";      // bool
    const std::string volume = "vl";    // bool

    /**
     * Effect chain per helper processor as ffmpeg
     * audio effect command, can be provided more than once
     *
     * Careful as creating too many helper
     * processor will severely add delay buffer
     * to audio playback
     */
    const std::string helper_chain = "ehl"; // str
    const std::string force = "frc";        // bool

    const std::string ytdlp_util_exe = "ytdex";    // str
    const std::string ytdlp_query = "ytdq";        // str
    const std::string ytdlp_max_entries = "ytdme"; // int
    const std::string ytdlp_lib_path = "ytdlibp";  // str

    const std::string gnuplot_cmd = "gplotcmd"; // str

    const std::string sys_cmd = "syscmd";           // str
    const std::string sys_max_out_len = "sysmol";   // uint64_t
    const std::string sys_no_stderr = "sysns";      // bool
    const std::string sys_w_stderr_mark = "syswsm"; // bool
} command_options_keys_t;

// update create_command_options impl below when changing this struct
struct command_options_t
{
    std::string command;

    std::string file_path;
    bool debug;
    std::string id;

    pid_t pid;
    int parent_read_fd;
    int parent_write_fd;
    int child_read_fd;
    int child_write_fd;

    std::string audio_stream_fifo_path;
    std::string guild_id;
    /**
     * Ready status
     */
    int ready;
    std::string seek;
    std::string audio_stream_stdin_path;
    std::string audio_stream_stdout_path;
    int volume;
    /**
     * A list with format `@effect_args@` without separator
     * Need to be parsed to processor_options_t helper_chain list
     */
    std::string helper_chain;

    /**
     * Not every command support this flag
     * Many will just ignore it
     */
    bool force;

    std::string ytdlp_util_exe;
    std::string ytdlp_query;
    int ytdlp_max_entries;
    std::string ytdlp_lib_path;

    std::string gnuplot_cmd;

    std::string sys_cmd;
    uint64_t sys_max_out_len;
    bool sys_no_stderr;
    bool sys_w_stderr_mark;

    // only for queue stuff like the shutdown queue
    time_t ts_queued;
};

inline command_options_t
create_command_options ()
{
    return { "", "", false, "", -1, -1, -1,    -1,    -1,
             "", "", false, "", "", "", 100,   "",    false,
             "", "", -1,    "", "", "", 10000, false, false };
}

inline int
assign_command_option_key_value (command_options_t &options,
                                 const std::string_view &opt,
                                 const std::string &value)
{
    // every value in command_options_keys_t should be handled here
    if (opt == command_options_keys_t.id)
        {
            options.id = value;
        }
    else if (opt == command_options_keys_t.command)
        {
            options.command = value;
        }
    else if (opt == command_options_keys_t.file_path)
        {
            options.file_path = value;
        }
    else if (opt == command_options_keys_t.debug)
        {
            // !TODO: remove every struct having this member
            // and use global debug state instead
            options.debug = value == "1";
            set_debug_state (options.debug);
        }
    else if (opt == command_options_keys_t.guild_id)
        {
            options.guild_id = value;
        }
    else if (opt == command_options_keys_t.ready)
        {
            options.ready = std::stoi (value);
        }
    else if (opt == command_options_keys_t.seek)
        {
            options.seek = value;
        }
    else if (opt == command_options_keys_t.volume)
        {
            options.volume = std::stoi (value);
        }
    else if (opt == command_options_keys_t.helper_chain)
        {
            options.helper_chain += '@' + value + '@';
        }
    else if (opt == command_options_keys_t.force)
        {
            options.force = value == "1";
        }
    else if (opt == command_options_keys_t.ytdlp_util_exe)
        {
            options.ytdlp_util_exe = value;
        }
    else if (opt == command_options_keys_t.ytdlp_query)
        {
            options.ytdlp_query = value;
        }
    else if (opt == command_options_keys_t.ytdlp_lib_path)
        {
            options.ytdlp_lib_path = value;
        }
    else if (opt == command_options_keys_t.ytdlp_max_entries)
        {
            options.ytdlp_max_entries = std::stoi (value);
        }
    else if (opt == command_options_keys_t.gnuplot_cmd)
        {
            options.gnuplot_cmd = value;
        }
    else if (opt == command_options_keys_t.sys_cmd)
        {
            options.sys_cmd = value;
        }
    else if (opt == command_options_keys_t.sys_max_out_len)
        {
            options.sys_max_out_len = value.empty () ? 1 : std::stoull (value);
        }
    else if (opt == command_options_keys_t.sys_no_stderr)
        {
            options.sys_no_stderr = value == "1";
        }
    else if (opt == command_options_keys_t.sys_w_stderr_mark)
        {
            options.sys_w_stderr_mark = value == "1";
        }

    return 0;
}

void command_queue_routine ();

void wait_for_command ();

void run_command_thread ();

int send_command (const std::string &cmd);

void wake ();

// default values are used in the main thread ONLY! You should specify
// write_fd and caller in child processes
void write_command (const std::string &cmd,
                    const int write_fd = *get_parent_write_fd (),
                    const char *caller = "child::command");

// should be called before send_command when setting value
// and use the return string as value
std::string sanitize_command_value (const std::string &value);

inline std::string
create_arg (const std::string &key, const std::string &value)
{
    return key + '=' + value + ';';
}

inline std::string
create_arg_sanitize_value (const std::string &key, const std::string &value)
{
    return create_arg (key, sanitize_command_value (value));
}

inline std::string
get_dbg_str_arg ()
{
    return create_arg (command_options_keys_t.debug,
                       get_debug_state () ? "1" : "0");
}

inline std::string
get_exit_command (const std::string &id)
{
    return create_arg (command_options_keys_t.id, id)
           + create_arg (command_options_keys_t.command,
                         command_execute_commands_t.shutdown);
}

// mostly internal use
std::string sanitize_command_key_value (const std::string &key_value);

void parse_command_to_options (const std::string &cmd,
                               command_options_t &options);

int wait_slave_ready (const std::string &id, int timeout);

int mark_slave_ready (const std::string &id, int status);

inline void
bail_out_afayc (const std::string &exit_cmd)
{
    const std::string force_exit_cmd
        = exit_cmd
          + command::create_arg (command::command_options_keys_t.force, "1");

    command::send_command (force_exit_cmd);
}

// send command and wait slave ready
// 0 on success, else error code of `wait_slave_ready()`
inline int
send_command_wr (const std::string &cmd, const std::string &exit_cmd,
                 const std::string &id, int timeout)
{
    namespace cw = child::worker;

    send_command (cmd);

    int status = wait_slave_ready (id, timeout);

    if (cw::should_bail_out_afayc (status))
        {
            bail_out_afayc (exit_cmd);
        }

    return status;
}

} // musicat::child::command

#endif // MUSICAT_CHILD_COMMAND_H
