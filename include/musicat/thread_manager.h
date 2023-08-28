#ifndef SHA_THREAD_MANAGER_H
#define SHA_THREAD_MANAGER_H

#include <deque>
#include <thread>

namespace musicat
{
namespace thread_manager
{

struct thread_data
{
    std::thread t;
    bool done;
};

void print_total_thread ();

void dispatch (std::thread &t);

void set_done ();

void join_done ();

void join_all ();

} // thread_manager
} // musicat

#endif // SHA_THREAD_MANAGER_H
