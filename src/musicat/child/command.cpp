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

} // command
} // child
} // musicat
