#include "musicat/thread_manager.h"
#include "musicat/musicat.h"

#include <deque>
#include <mutex>
#include <stdio.h>
#include <thread>

namespace musicat
{
namespace thread_manager
{
std::mutex _ns_mutex;
std::deque<thread_data> _threads = {};

void
print_total_thread ()
{
    fprintf (stderr, "[INFO] Total active thread: %ld\n", _threads.size ());
}

void
dispatch (std::thread &t)
{
    if (!get_running_state ())
        {
            std::cerr << "[ERROR] Shouldn't spawn new thread when exiting, "
                         "detaching this one while you fix the code! "
                      << t.get_id () << '\n';

            t.detach ();

            return;
        }

    const bool debug = get_debug_state ();
    std::lock_guard lk (_ns_mutex);

    if (debug)
        {
            std::cerr << "[INFO] New thread spawned: " << t.get_id () << "\n";
        }

    thread_data td = { std::move (t), false };

    _threads.push_back (std::move (td));

    if (debug)
        {
            print_total_thread ();
        }
}

void
set_done ()
{
    std::lock_guard lk (_ns_mutex);

    const auto id = std::this_thread::get_id ();

    auto i = _threads.begin ();

    while (i != _threads.end ())
        {
            if (i->t.get_id () != id)
                {
                    i++;
                    continue;
                }

            if (!i->done)
                {
                    i->done = true;
                }

            break;
        }

    if (get_debug_state ())
        {
            std::cerr << "[INFO] Thread done: " << id << "\n";
            print_total_thread ();
        }
}

void
join_done ()
{
    std::lock_guard lk (_ns_mutex);

    auto i = _threads.begin ();

    size_t done = 0;
    size_t joined = 0;
    while (i != _threads.end ())
        {
            if (!i->done)
                {
                    i++;
                    continue;
                }

            if (i->t.joinable ())
                {
                    i->t.join ();
                    joined++;
                }

            i = _threads.erase (i);
            done++;
        }

    if (done && get_debug_state ())
        {
            fprintf (stderr, "[INFO] Total thread done: %ld\n", done);
            fprintf (stderr, "[INFO] Total thread done and joined: %ld\n",
                     joined);

            print_total_thread ();
        }
}

void
join_all ()
{
    const bool debug = get_debug_state ();

    // manually lock and unlock mutex for this specific case
    // where the thread can call set_done without deadlocking
    // on exit
    _ns_mutex.lock ();

    if (debug)
        {
            fprintf (stderr, "[INFO] Joining %ld threads...\n",
                     _threads.size ());
        }

    size_t joined = 0;
    auto i = _threads.begin ();
    while (i != _threads.end ())
        {
            if (i->t.joinable ())
                {
                    _ns_mutex.unlock ();
                    // thread should guarantee to never modify _threads
                    // when exiting
                    i->t.join ();
                    _ns_mutex.lock ();
                    joined++;

                    i = _threads.erase (i);
                }
            else
                i++;
        }

    if (debug)
        {
            fprintf (stderr, "[INFO] Total joined thread: %ld\n", joined);
            print_total_thread ();
        }

    _ns_mutex.unlock ();
}

} // thread_manager
} // musicat
