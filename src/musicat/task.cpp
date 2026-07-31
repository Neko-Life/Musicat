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
