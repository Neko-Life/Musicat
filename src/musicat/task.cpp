#include <condition_variable>
#include <cstdint>
#include <functional>

#include "dpp/timer.h"
#include "musicat/musicat.h"
#include "musicat/util.h"

namespace musicat::task
{

void
run (const std::function<void ()> &&fn)
{
    auto *cluster = get_client_ptr ();
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

static std::deque<blocking_task_t> blocking_tasks;
static std::mutex blocking_tasks_m;
static std::condition_variable blocking_tasks_cv;

void
run_may_block (const std::function<void ()> &&fn, const std::function<bool ()> &&check_blocking)
{
    std::lock_guard lk (blocking_tasks_m);
    blocking_tasks.push_back ({ fn, check_blocking, 0, false });
}

// main loop routine
void
check_blocking_task ()
{
    std::deque<blocking_task_t> tasks_ready;
    {
        std::lock_guard lk (blocking_tasks_m);
        auto i = blocking_tasks.begin ();
        while (i != blocking_tasks.end ())
            {
                if (i->will_block ())
                    {
                        i++;
                        continue;
                    }

                tasks_ready.push_back (std::move (*i));
                i = blocking_tasks.erase (i);
            }
    }

    for (auto &task : tasks_ready)
        if (task.on_main)
            {
                task.run ();
                blocking_tasks_cv.notify_all ();
            }
        else
            run (std::move (task.run));
}

void
run_once (const std::function<void ()> &&fn, uint64_t after)
{
    auto *cluster = get_client_ptr ();
    if (!cluster)
        return;

    cluster->start_timer (
        [fn] (dpp::timer handle)
            {
                auto *cluster = get_client_ptr ();
                if (!cluster)
                    return;

                cluster->stop_timer (handle);

                fn ();
            },
        after);
}

void
run_on_main_and_wait (const std::function<void ()> &&fn)
{
    uint64_t cts = (uint64_t)util::get_current_ts () + util::get_random_number ();
    std::unique_lock lk (blocking_tasks_m);
    blocking_tasks.push_back ({ fn, [] () { return false; }, cts, true });

    blocking_tasks_cv.wait (lk,
                            [cts] ()
                                {
                                    return std::find_if (blocking_tasks.begin (), blocking_tasks.end (),
                                                         [cts] (blocking_task_t &task) { return task.id == cts; })
                                           == blocking_tasks.end ();
                                });
}

} // namespace musicat::task
