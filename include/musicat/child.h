#ifndef MUSICAT_CHILD_H
#define MUSICAT_CHILD_H

#include <string>
#ifdef _WIN32

#else
#include <semaphore.h>
#endif

#define CMD_BUFSIZE BUFSIZ

namespace musicat
{
namespace child
{
enum child_error_t
{
    SUCCESS,
    ERR_CWORKER,
    ERR_CPIPE,
};

int init ();

int *get_parent_write_fd ();
int *get_parent_read_fd ();

void shutdown ();

// should be called by parent process
std::string get_sem_key (const std::string &key);

#ifdef _SEMAPHORE_H
// should be called by parent process
sem_t *create_sem (const std::string &full_key);

// should be called by child process
int do_sem_post (sem_t *sem);

int clear_sem (sem_t *sem, const std::string &full_key);

// should be called by parent process
int do_sem_wait (sem_t *sem, const std::string &full_key);
#endif

} // child
} // musicat

#endif // MUSICAT_CHILD_H
