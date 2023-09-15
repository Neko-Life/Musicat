#ifndef MUSICAT_CHILD_H
#define MUSICAT_CHILD_H

#define CMD_BUFSIZE BUFSIZ / 2

namespace musicat
{
namespace child
{
extern int pm_write_fd, pm_read_fd;

enum child_error_t
{
    SUCCESS,
    ERR_CWORKER,
    ERR_CPIPE,
};

int init ();

int get_parent_write_fd ();

void shutdown ();

} // child
} // musicat

#endif // MUSICAT_CHILD_H
