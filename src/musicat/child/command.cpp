#include "musicat/child/command.h"
#include "musicat/child.h"
#include "musicat/musicat.h"
#include "musicat/thread_manager.h"
#include <mutex>

namespace musicat
{
namespace child
{
namespace command
{

std::deque<std::string> command_queue;
std::mutex command_mutex;
std::condition_variable command_cv;

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

    thread_manager::dispatch (command_thread);
}

int
send_command (std::string &cmd)
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

void
write_command (std::string &cmd)
{
    const size_t cmd_size = cmd.size ();
    if (cmd_size > CMD_BUFSIZE)
        {
            fprintf (stderr,
                     "[child::command ERROR] Command size bigger than read "
                     "buffer size: %ld > %d\n",
                     cmd_size, CMD_BUFSIZE);
            fprintf (stderr, "[child::command ERROR] Dropping command: %s\n",
                     cmd.c_str ());
            return;
        }

    cmd.resize (CMD_BUFSIZE, '\0');

    int write_fd = get_parent_write_fd ();
    write (write_fd, cmd.c_str (), CMD_BUFSIZE);
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

} // command
} // child
} // musicat
