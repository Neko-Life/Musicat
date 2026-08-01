#include <functional>

#include "dpp/timer.h"
#include "musicat/musicat.h"

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
};

static std::deque<blocking_task_t> blocking_tasks;
static std::mutex blocking_tasks_m;

void
run_may_block (const std::function<void ()> &&fn, const std::function<bool ()> &&check_blocking)
{
    std::lock_guard lk (blocking_tasks_m);
    blocking_tasks.push_back ({ fn, check_blocking });
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

} // namespace musicat::task
