#ifndef MUSICAT_TASK_H
#define MUSICAT_TASK_H

#include <functional>

namespace musicat::task
{

void run (const std::function<void ()> &&fn);
// run task that may block
// calls check_blocking() to determine whether the task will block
// if it won't block then continue to run it
void run_may_block (const std::function<void ()> &&fn, const std::function<bool ()> &&check_blocking);

// main loop routine
void check_blocking_task();

// run after (in second)
void run_once (const std::function<void ()> &&fn, uint64_t after);

} // namespace musicat::task

#endif // MUSICAT_TASK_H
