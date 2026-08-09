#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>

#include "dpp/timer.h"
#include "musicat/musicat.h"
#include "musicat/util.h"

namespace musicat::task
{

void
run (const std::function<void ()> &&fn)
{
    auto *cluster = get_cluster_ptr ();
    if (!cluster)
        return;

    // match dpp default priority
    cluster->queue_work (0, std::move (fn));
}

struct blocking_task_t
{
    std::function<void ()> run;
    std::function<bool ()> will_block;
    uint64_t id;
    bool on_main;
};

using deque_blocking_task_t = std::deque<blocking_task_t>;
static condition_container<deque_blocking_task_t> blocking_tasks;

void
run_may_block (const std::function<void ()> &&fn, const std::function<bool ()> &&check_blocking)
{
    auto lk = blocking_tasks.acquire ();
    blocking_tasks.get ().push_back ({ fn, check_blocking, 0, false });
}

// main loop routine
void
check_blocking_task ()
{
    std::deque<blocking_task_t> tasks_ready;
    {
        auto lk = blocking_tasks.acquire ();
        auto i = blocking_tasks.get ().begin ();
        while (i != blocking_tasks.get ().end ())
            {
                if (i->will_block ())
                    {
                        i++;
                        continue;
                    }

                tasks_ready.push_back (std::move (*i));
                i = blocking_tasks.get ().erase (i);
            }
    }

    for (auto &task : tasks_ready)
        if (task.on_main)
            {
                task.run ();
                blocking_tasks.cv.notify_all ();
            }
        else
            run (std::move (task.run));
}

void
run_once (const std::function<void ()> &&fn, uint64_t after)
{
    auto *cluster = get_cluster_ptr ();
    if (!cluster)
        return;

    cluster->start_timer (
        [fn] (dpp::timer handle)
            {
                auto *cluster = get_cluster_ptr ();
                if (!cluster)
                    return;

                cluster->stop_timer (handle);

                fn ();
            },
        after);
}

void
run_on_main (const std::function<void ()> &&fn)
{
    uint64_t cts = (uint64_t)util::get_current_ts () + util::get_random_number ();
    auto lk = blocking_tasks.acquire ();
    blocking_tasks.get ().push_back ({ fn, [] () { return false; }, cts, true });
}

void
run_on_main_and_wait (const std::function<void ()> &&fn)
{
    uint64_t cts = (uint64_t)util::get_current_ts () + util::get_random_number ();
    auto lk = blocking_tasks.acquire ();
    blocking_tasks.get ().push_back ({ fn, [] () { return false; }, cts, true });

    blocking_tasks.cv.wait (lk,
                            [cts] ()
                                {
                                    return std::find_if (blocking_tasks.get ().begin (), blocking_tasks.get ().end (),
                                                         [cts] (blocking_task_t &task) { return task.id == cts; })
                                           == blocking_tasks.get ().end ();
                                });
}

} // namespace musicat::task
