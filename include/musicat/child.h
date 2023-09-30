#ifndef MUSICAT_CHILD_H
#define MUSICAT_CHILD_H

#define CMD_BUFSIZE BUFSIZ / 2

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

} // child
} // musicat

#endif // MUSICAT_CHILD_H
