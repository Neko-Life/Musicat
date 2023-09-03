#ifndef MUSICAT_CHILD_WORKER_H
#define MUSICAT_CHILD_WORKER_H

#include <string>

namespace musicat
{
namespace child
{
namespace worker
{

void set_fds (int r, int w);

void run ();

/**
 * @brief Create a one way pipe, read_fd and write_fd
 */
std::pair<int, int> create_pipe ();

} // worker
} // child
} // musicat

#endif // MUSICAT_CHILD_WORKER_H
