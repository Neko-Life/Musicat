#include "musicat/runtime_cli.h"
#include "musicat/mctrack.h"
#include "musicat/musicat.h"
#include "musicat/player.h"
#include "musicat/thread_manager.h"
#include <sys/poll.h>

using cmd_args_t = std::vector<std::string>;

struct command_entry_t
{
    const char *name;
    const char *alias;
    const char *description;
    int (*const handler) (const cmd_args_t &args);
};

////////////////////////////////////////////////////////////////////////////////

static void
_print_pad (size_t len)
{
    if ((long long)len < 0LL)
        return;

    for (size_t i = 0; i < len; i++)
        {
            fprintf (stderr, " ");
        }
}

static bool
_cmd_end (const command_entry_t &cmd)
{
    return !cmd.name && !cmd.alias && !cmd.description && !cmd.handler;
}

////////////////////////////////////////////////////////////////////////////////

namespace musicat::runtime_cli
{
static bool attached = false;
const command_entry_t *commands_ptr = NULL;
std::mutex ns_mutex; // EXTERN_VARIABLE

static size_t padding_command = 0;
static size_t padding_alias = 0;

////////////////////////////////////////////////////////////////////////////////

static int
help_cmd (const cmd_args_t &args)
{
    std::lock_guard lk (ns_mutex);
    if (!commands_ptr)
        {
            fprintf (stderr,
                     "[runtime_cli::help_cmd ERROR] Commands ptr null\n");

            return 1;
        }

    fprintf (stderr, "Usage: [command] [args] <ENTER>\n\n");

    size_t i = 0;
    while (true)
        {
            auto cmd = commands_ptr[i++];

            bool end = _cmd_end (cmd);
            if (end)
                break;

            size_t name_len = cmd.name ? strlen (cmd.name) : 0;
            size_t alias_len = cmd.alias ? strlen (cmd.alias) : 0;

            // print only new line if there's dummy handler
            if (!name_len && !alias_len && !cmd.description)
                {
                    fprintf (stderr, "\n");
                    continue;
                }

            if (name_len)
                fprintf (stderr, "%s", cmd.name);

            _print_pad (padding_command - name_len);

            fprintf (stderr, (name_len || alias_len) ? ":" : " ");

            const size_t pad_a = padding_alias - alias_len;

            _print_pad (pad_a / 2);

            if (alias_len)
                fprintf (stderr, "%s", cmd.alias);

            _print_pad ((size_t)ceil ((double)pad_a / (double)2.0));

            if (cmd.description)
                fprintf (stderr, "%s %s\n",
                         (name_len || alias_len) ? ":" : " ", cmd.description);
        }

    fprintf (stderr, "\n");

    return 0;
}

static int
debug_cmd (const cmd_args_t &args)
{
    set_debug_state (!get_debug_state ());
    return 0;
}

static int
clear_cmd (const cmd_args_t &args)
{
    system ("clear");
    return 0;
}

static int
shutdown_cmd (const cmd_args_t &args)
{
    fprintf (stderr, "Shutting down...\n");
    set_running_state (false);
    return 0;
}

static int
list_effect_states (const cmd_args_t &args)
{
    std::lock_guard lk (player::effect_states_list_m);

    auto efs = player::get_effect_states_list ();

    for (auto ef : *efs)
        {
            auto gid = ef->guild_player->guild_id;

            auto g = dpp::find_guild (gid);
            std::string gstr = g ? g->name : "[not_found]";

            std::cerr << gstr << " (" << gid
                      << "):\nTrack: " << mctrack::get_title (ef->track)
                      << '\n'
                      << "Command fd: " << ef->command_fd << "\n==========\n";
        }

    return 0;
}

static int
effect_states_send_command (const cmd_args_t &args)
{

    return 0;
}

// !TODO: more cmd? maybe stats/utility

////////////////////////////////////////////////////////////////////////////////

/**
 * Commands list, name alias description handler
 *
 * description only means it's the continuation of above's description
 * handler only means to print new blank line
 *
 * !Always check and adjust aliases to not clash with one another, commands are
 * matched using `starts_with`
 */
inline constexpr const command_entry_t commands[] = {
    { "command", "alias", "description", NULL },
    { "help", "-h", "Print this message", help_cmd },
    { "debug", "-d", "Toggle debug mode", debug_cmd },
    { NULL, NULL, "Debug mode prints everything for debugging purpose", NULL },
    { "clear", "-c", "Clear console", clear_cmd },
    { "shutdown", NULL, "Shutdown Musicat", shutdown_cmd },
    { "list effect states", "-ls es", "List currently active effect states",
      list_effect_states },
    { NULL, NULL, NULL, NULL },
};

////////////////////////////////////////////////////////////////////////////////

bool
get_attached_state ()
{
    std::lock_guard lk (ns_mutex);
    return attached;
}

bool
set_attached_state (bool s)
{
    std::lock_guard lk (ns_mutex);
    attached = s;
    return attached;
}

/*
'   inter   com iah h   '
'inter  com iah h    '
'inter  com iah h'
'   inter com iah h'
*/

cmd_args_t
parse_args_str (const std::string &args_str)
{
    cmd_args_t ret = {};

    size_t max_i = args_str.length ();

    // return early when no arg length
    if (!max_i)
        return ret;

    // start index
    size_t s_i = 0;
    // valid char found
    bool f = false;
    for (size_t i = 0; i < args_str.length (); i++)
        {
            char c = args_str[i];

            // valid char
            if (c != ' ')
                {
                    // never had found valid char
                    if (!f)
                        {
                            // set current index as the first valid char
                            s_i = i;
                            // set found valid char
                            f = true;
                        }

                    continue;
                }

            // found space

            // never got valid char, do nothing
            if (!f)
                continue;

            // first valid char found
            // push the word
            ret.push_back (args_str.substr (s_i, i - s_i));
            // reset found state
            f = false;
        }

    // end of string
    // check for valid char found
    if (f)
        {
            // push word at the end of string
            ret.push_back (args_str.substr (s_i));
            // reset found state
            // f = false;
        }

    // debug
    /*
    std::cerr << "args: ";
    for (const auto &s : ret)
        {
            std::cerr << '`' << s << "` ";
        }
    std::cerr << "\n";
    */

    return ret;
}

void
handle_command (const std::string &cmd_str)
{
    for (const command_entry_t &command : commands)
        {
            bool end = _cmd_end (command);
            if (end)
                break;

            if (!command.handler)
                continue;

            bool no_name = !command.name;
            bool no_alias = !command.alias;

            if (no_name && no_alias)
                {
                    continue;
                }

            bool match = false, is_alias = false;

            if (!no_alias)
                {
                    match = cmd_str.find (command.alias) == 0;
                    if (match)
                        {
                            is_alias = true;
                        }
                }

            if (!match && !no_name)
                match = cmd_str.find (command.name) == 0;

            if (!match)
                continue;

            std::string args_str = cmd_str.substr (
                is_alias ? strlen (command.alias) : strlen (command.name));

            int status = command.handler (parse_args_str (args_str));

            if (status != 0)
                {
                    fprintf (stderr, "[%s] %d\n", cmd_str.c_str (), status);
                }
        }
}

////////////////////////////////////////////////////////////////////////////////

class AttachedReset
{
  public:
    ~AttachedReset ()
    {
        std::lock_guard lk (ns_mutex);
        attached = false;
    }
};

void
load_commands ()
{
    commands_ptr = commands;

    for (const command_entry_t &cmd : commands)
        {
            bool end = _cmd_end (cmd);
            if (end)
                break;

            size_t len_command = (cmd.name ? strlen (cmd.name) : 0) + 1U;
            size_t len_alias = (cmd.alias ? strlen (cmd.alias) : 0) + 2U;

            if (len_command > padding_command)
                padding_command = len_command;

            if (len_alias > padding_alias)
                padding_alias = len_alias;
        }
}

int
attach_listener ()
{
    std::lock_guard lk (ns_mutex);

    if (attached == true)
        {
            fprintf (stderr, "[runtime_cli::attach_listener ERROR] "
                             "Listener already attached!\n");

            return 1;
        }
    attached = true;
    load_commands ();

    fprintf (stderr, "[INFO] Enter `-d` to toggle debug mode\n");

    std::thread stdin_listener ([] () {
        thread_manager::DoneSetter tmds;
        AttachedReset ar;

        struct pollfd stdinpfds[1];
        stdinpfds[0].events = POLLIN;
        stdinpfds[0].fd = STDIN_FILENO;

        while (get_running_state ())
            {
                std::string cmd_str;

                // poll for 2 seconds every iteration
                const int read_has_event = poll (stdinpfds, 1, 2000);

                std::string codes = "";
                if (stdinpfds[0].revents & POLLERR)
                    {
                        codes += "POLLERR";
                    }

                if (stdinpfds[0].revents & POLLHUP)
                    {
                        codes += " POLLHUP";
                    }

                if (stdinpfds[0].revents & POLLNVAL)
                    {
                        codes += " POLLNVAL";
                    }

                if (!codes.empty ())
                    {
                        fprintf (stderr,
                                 "[runtime_cli::stdin_listener ERROR] %s ",
                                 codes.c_str ());

                        perror ("");
                        fprintf (stderr, "Aborting thread...\n");
                        break;
                    }

                bool read_ready
                    = (read_has_event > 0) && (stdinpfds[0].revents & POLLIN);

                if (read_ready)
                    std::getline (std::cin, cmd_str);
                else
                    continue;

                if (cmd_str.empty ())
                    continue;

                handle_command (cmd_str);
            }
    });

    thread_manager::dispatch (stdin_listener);

    return 0;
}

} // musicat::runtime_cli
