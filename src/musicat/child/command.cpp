#include "musicat/child/command.h"
#include "musicat/child.h"
#include "musicat/musicat.h"
#include "musicat/thread_manager.h"
#include <mutex>

// this namespace is mostly still executed in main program/thread
// with exception of some util function
namespace musicat::child::command
{

std::deque<std::string> command_queue;
std::mutex command_mutex;
std::condition_variable command_cv;

std::map<std::string, std::pair<bool, int> > slave_ready_queue;
std::mutex sr_m;
std::condition_variable sr_cv;

int
set_option (command_options_t &options, const std::string &cmd_option)
{
    std::string opt = "";
    std::string value = "";

    bool filling_value = false;
    bool include_next_special = false;
    for (char c : cmd_option)
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

    return assign_command_option_key_value (options, opt, value);
}

void
command_queue_routine ()
{
    std::lock_guard lk (command_mutex);

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
    std::unique_lock ulk (command_mutex);

    command_cv.wait (ulk, [] () {
        return !command_queue.empty () || !get_running_state ();
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

// run in main program thread
void
run_command_thread ()
{
    std::thread command_thread ([] () {
        thread_manager::DoneSetter tmds;

        while (get_running_state ())
            {
                command_queue_routine ();
                wait_for_command ();
            }
    });

    std::thread notify_thread ([] () {
        thread_manager::DoneSetter tmds;

        char read_buf[CMD_BUFSIZE + 1];
        size_t read_size = 0;

        while (get_running_state ()
               && ((read_size
                    = read (*get_parent_read_fd (), read_buf, CMD_BUFSIZE))
                   > 0))
            {
                read_buf[read_size] = '\0';

                if (get_debug_state ())
                    fprintf (stderr,
                             "[child::command] Received "
                             "notification: %s\n",
                             read_buf);

                const std::string read_str (read_buf);

                command_options_t options = create_command_options ();
                parse_command_to_options (read_str, options);

                // !TODO: handle options
                handle_child_message (options);
            }
    });

    thread_manager::dispatch (command_thread);
    thread_manager::dispatch (notify_thread);
}

int
send_command (const std::string &cmd)
{
    {
        std::lock_guard lk (command_mutex);

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
    = "[%s child::command::write_command ERROR] Dropping command: `%s`\n";
inline constexpr const char *wccfmt3
    = "[%s child::command::write_command] fd %d: `%s`\n";

void
write_command (const std::string &cmd, const int write_fd, const char *caller)
{
    if (write_fd < 0)
        {
            fprintf (stderr, wcwffmt, caller, write_fd);
            return;
        }

    size_t cmd_size = cmd.size ();
    if (cmd_size > CMD_BUFSIZE)
        {
            fprintf (stderr, wccfmt, caller, cmd_size, CMD_BUFSIZE);
            fprintf (stderr, wccfmt2, caller, cmd.c_str ());
            return;
        }

    std::string cpy (cmd);
    cpy.resize (CMD_BUFSIZE, '\0');

    if (get_debug_state ())
        fprintf (stderr, wccfmt3, caller, write_fd, cpy.c_str ());

    write (write_fd, cpy.c_str (), CMD_BUFSIZE);
}

static inline const std::string to_sanitize_command_value ("\\=;");

bool
should_sanitize (char tc)
{
    for (char c : to_sanitize_command_value)
        {
            if (c == tc)
                return true;
        }

    return false;
}

inline constexpr const char *ccscv
    = "[child::command::sanitize_command_value] value: `%s`\n";
inline constexpr const char *ccscv2
    = "[child::command::sanitize_command_value] new_value: `%s`\n";

// should be called before send_command when setting value
std::string
sanitize_command_value (const std::string &value)
{
    bool debug = get_debug_state ();

    if (debug)
        fprintf (stderr, ccscv, value.c_str ());

    std::string new_value = "";

    for (char c : value)
        {
            if (should_sanitize (c))
                {
                    new_value += '\\';
                }

            new_value += c;
        }

    if (debug)
        fprintf (stderr, ccscv2, new_value.c_str ());

    return new_value;
}

inline constexpr const char *ccsckv
    = "[child::command::sanitize_command_key_value] key_value: `%s`\n";
inline constexpr const char *ccsckv2
    = "[child::command::sanitize_command_key_value] new_key_value: `%s`\n";

std::string
sanitize_command_key_value (const std::string &key_value)
{
    bool debug = get_debug_state ();

    if (debug)
        fprintf (stderr, ccsckv, key_value.c_str ());

    std::string new_key_value = "";

    bool should_check = false;
    for (char c : key_value)
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

    if (debug)
        fprintf (stderr, ccsckv2, new_key_value.c_str ());

    return new_key_value;
}

void
parse_command_to_options (const std::string &cmd, command_options_t &options)
{
    bool include_next_special = false;
    std::string temp_str = "";
    for (char c : cmd)
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
wait_slave_ready (const std::string &id, int timeout)
{
    const bool debug = get_debug_state ();

    {
        std::lock_guard lk (sr_m);
        auto i = slave_ready_queue.find (id);
        if (i != slave_ready_queue.end ())
            {
                const int ret = i->second.second;
                slave_ready_queue.erase (i);

                if (debug)
                    {
                        fprintf (stderr,
                                 "[child::command::wait_slave_ready] Slave "
                                 "`%s` done ready with status %d\n",
                                 id.c_str (), ret);
                    }

                return ret;
            }
    }

    {
        std::lock_guard lk (sr_m);
        slave_ready_queue.insert_or_assign (id, std::make_pair (false, -1));

        if (debug)
            {
                fprintf (stderr,
                         "[child::command::wait_slave_ready] Slave `%s` ready "
                         "state initialized with status -1\n",
                         id.c_str ());
            }
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

    std::lock_guard lk (sr_m);
    auto i = slave_ready_queue.find (id);
    if (i == slave_ready_queue.end ())
        {
            if (debug)
                {
                    fprintf (
                        stderr,
                        "[child::command::wait_slave_ready] Slave `%s` ready "
                        "state missing! Probably killed or init error\n",
                        id.c_str ());
                }

            return -1;
        }

    const int ret = i->second.second;
    slave_ready_queue.erase (i);

    if (debug)
        {
            fprintf (stderr,
                     "[child::command::wait_slave_ready] Slave `%s` now ready "
                     "with status %d\n",
                     id.c_str (), ret);
        }

    return ret;
}

int
mark_slave_ready (const std::string &id, int status)
{
    {
        std::lock_guard lk (sr_m);
        slave_ready_queue.insert_or_assign (id, std::make_pair (true, status));
    }

    sr_cv.notify_all ();

    return 0;
}

} // musicat::child::command
