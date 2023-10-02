#include "musicat/child/command.h"
#include "musicat/child.h"
#include "musicat/musicat.h"
#include "musicat/thread_manager.h"
#include <chrono>
#include <mutex>

namespace musicat
{
namespace child
{
// this namespace is mostly still executed in main program/thread
// with exception of some util function
namespace command
{

std::deque<std::string> command_queue;
std::mutex command_mutex;
std::condition_variable command_cv;

std::map<std::string, std::pair<bool, int> > slave_ready_queue;
std::mutex sr_m;
std::condition_variable sr_cv;

int
set_option (command_options_t &options, std::string &cmd_option)
{
    std::string opt = "";
    std::string value = "";

    bool filling_value = false;
    bool include_next_special = false;
    for (const char &c : cmd_option)
        {
            if (!include_next_special)
                {
                    if (c == '\\')
                        {
                            include_next_special = true;
                            continue;
                        }

                    if (c == '=')
                        {
                            filling_value = true;
                            continue;
                        }
                }
            else
                include_next_special = false;

            if (filling_value)
                {
                    value += c;
                    continue;
                }

            opt += c;
        }

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
            options.debug = value == "1";
        }
    else if (opt == command_options_keys_t.guild_id)
        {
            options.guild_id = value;
        }
    else if (opt == command_options_keys_t.ready)
        {
            options.ready = atoi (value.c_str ());
        }
    else if (opt == command_options_keys_t.seek)
        {
            options.seek = value;
        }
    else if (opt == command_options_keys_t.volume)
        {
            options.volume = atoi (value.c_str ());
        }
    else if (opt == command_options_keys_t.helper_chain)
        {
            options.helper_chain += '@' + value + '@';
        }
    else if (opt == command_options_keys_t.force)
        {
            options.force = value == "1";
        }

    return 0;
}

void
command_queue_routine ()
{
    std::lock_guard<std::mutex> lk (command_mutex);

    auto i = command_queue.begin ();
    while (i != command_queue.end ())
        {
            write_command (*i);

            // !TODO: read for status and write it to main parent

            i = command_queue.erase (i);
        }
}

void
wait_for_command ()
{
    std::unique_lock<std::mutex> ulk (command_mutex);

    command_cv.wait (ulk, [] () {
        return (command_queue.size () > 0) || !get_running_state ();
    });
}

void
handle_child_message (command_options_t &options)
{
    if (options.command == command_options_keys_t.ready)
        {
            mark_slave_ready (options.id, options.ready);
        }
}

void
run_command_thread ()
{
    std::thread command_thread ([] () {
        while (get_running_state ())
            {
                command_queue_routine ();
                wait_for_command ();
            }

        thread_manager::set_done ();
    });

    // !TODO: create a thread to read from worker
    // and do stuff like notify cv
    std::thread notify_thread ([] () {
        char read_buf[CMD_BUFSIZE + 1];
        size_t read_size = 0;

        while (get_running_state ()
               && ((read_size
                    = read (*get_parent_read_fd (), read_buf, CMD_BUFSIZE))
                   > 0))
            {
                read_buf[CMD_BUFSIZE] = '\0';

                fprintf (stderr,
                         "[child::command] Received "
                         "notification: %s\n",
                         read_buf);

                std::string read_str (read_buf);

                command_options_t options = create_command_options ();
                parse_command_to_options (read_str, options);

                // !TODO: handle options
                handle_child_message (options);
            }

        thread_manager::set_done ();
    });

    thread_manager::dispatch (command_thread);
    thread_manager::dispatch (notify_thread);
}

int
send_command (const std::string &cmd)
{
    {
        std::lock_guard<std::mutex> lk (command_mutex);

        command_queue.push_back (cmd);
    }

    wake ();

    return SUCCESS;
}

void
wake ()
{
    command_cv.notify_all ();
}

inline constexpr const char *wcwffmt
    = "[%s child::command::write_command ERROR] Invalid fd %d\n";
inline constexpr const char *wccfmt
    = "[%s child::command::write_command ERROR] Command size bigger than read "
      "buffer size: %ld > %d\n";
inline constexpr const char *wccfmt2
    = "[%s child::command::write_command ERROR] Dropping command: %s\n";

void
write_command (const std::string &cmd, const int write_fd, const char *caller)
{
    if (write_fd < 0)
        {
            fprintf (stderr, wcwffmt, caller, write_fd);
            return;
        }

    const size_t cmd_size = cmd.size ();
    if (cmd_size > CMD_BUFSIZE)
        {
            fprintf (stderr, wccfmt, caller, cmd_size, CMD_BUFSIZE);
            fprintf (stderr, wccfmt2, caller, cmd.c_str ());
            return;
        }

    std::string cpy (cmd);
    cpy.resize (CMD_BUFSIZE, '\0');

    write (write_fd, cpy.c_str (), CMD_BUFSIZE);
}

static const std::string to_sanitize_command_value ("\\=;");

bool
should_sanitize (const char &tc)
{
    for (const char &c : to_sanitize_command_value)
        {
            if (c == tc)
                return true;
        }

    return false;
}

// should be called before send_command when setting value
std::string
sanitize_command_value (const std::string &value)
{
    // fprintf (stderr, "value: %s\n", value.c_str ());
    std::string new_value = "";

    for (const char &c : value)
        {
            if (should_sanitize (c))
                {
                    new_value += '\\';
                }

            new_value += c;
        }

    // fprintf (stderr, "new_value: %s\n", new_value.c_str ());

    return new_value;
}

std::string
sanitize_command_key_value (const std::string &key_value)
{
    // fprintf (stderr, "key_value: %s\n", key_value.c_str ());
    std::string new_key_value = "";

    bool should_check = false;
    for (const char &c : key_value)
        {
            if (should_check && should_sanitize (c))
                {
                    new_key_value += '\\';
                }

            if (!should_check && c == '=')
                {
                    should_check = true;
                }

            new_key_value += c;
        }

    // fprintf (stderr, "new_key_value: %s\n", new_key_value.c_str ());

    return new_key_value;
}

void
parse_command_to_options (const std::string &cmd, command_options_t &options)
{
    bool include_next_special = false;
    std::string temp_str = "";
    for (const char &c : cmd)
        {
            if (!include_next_special)
                {
                    if (c == '\\')
                        {
                            include_next_special = true;
                            continue;
                        }

                    if (c == ';')
                        {
                            std::string opt_str
                                = command::sanitize_command_key_value (
                                    temp_str);
                            set_option (options, opt_str);
                            temp_str = "";
                            continue;
                        }
                }
            else
                include_next_special = false;

            temp_str += c;
        }

    if (!temp_str.empty ())
        {
            std::string opt_str
                = command::sanitize_command_key_value (temp_str);
            set_option (options, opt_str);
        }
}

int
wait_slave_ready (std::string &id, const int timeout)
{
    {
        std::lock_guard<std::mutex> lk (sr_m);
        auto i = slave_ready_queue.find (id);
        if (i != slave_ready_queue.end ())
            {
                const int ret = i->second.second;
                slave_ready_queue.erase (i);

                return ret;
            }
    }

    {
        std::lock_guard<std::mutex> lk (sr_m);
        slave_ready_queue.insert_or_assign (id, std::make_pair (false, -1));
    }
    {
        std::unique_lock ulk (sr_m);
        sr_cv.wait_until (ulk,
                          std::chrono::system_clock::now ()
                              + std::chrono::seconds (timeout),
                          [id] () {
                              auto i = slave_ready_queue.find (id);
                              return i == slave_ready_queue.end ()
                                     || i->second.first == true;
                          });
    }

    std::lock_guard<std::mutex> lk (sr_m);
    auto i = slave_ready_queue.find (id);
    if (i == slave_ready_queue.end ())
        {
            return -1;
        }

    const int ret = i->second.second;
    slave_ready_queue.erase (i);

    return ret;
}

int
mark_slave_ready (std::string &id, const int status)
{
    {
        std::lock_guard<std::mutex> lk (sr_m);
        slave_ready_queue.insert_or_assign (id, std::make_pair (true, status));
    }

    sr_cv.notify_all ();

    return 0;
}

} // command
} // child
} // musicat
