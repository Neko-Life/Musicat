#ifndef MUSICAT_CHILD_WORKER_H
#define MUSICAT_CHILD_WORKER_H

namespace musicat
{
namespace child
{
namespace worker
{

void set_fds (int r, int w);

void run ();

} // worker
} // child
} // musicat

#endif // MUSICAT_CHILD_WORKER_H
