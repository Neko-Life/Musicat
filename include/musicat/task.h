#ifndef MUSICAT_TASK_H
#define MUSICAT_TASK_H

#include <functional>

namespace musicat::task
{

void run (const std::function<void ()> &&fn);
// run after (in second)
void run_once (const std::function<void ()> &&fn, uint64_t after);

} // namespace musicat::task

#endif // MUSICAT_TASK_H
