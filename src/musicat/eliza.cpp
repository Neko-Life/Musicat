#include <stdio.h>
#include <sys/stat.h>

namespace musicat::eliza
{

int
create_eliza_dir ()
{
    struct stat ofile_stat;

    bool serr = false;
    if ((serr = (stat ("eliza", &ofile_stat) != 0))
        || !S_ISDIR (ofile_stat.st_mode))
        {
            if (serr)
                perror ("eliza");
        }

    return 0;
}

} // musicat::eliza
