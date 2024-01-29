#include "musicat/child/ytdlp.h"

namespace musicat::child::ytdlp
{

std::string
get_ytdout_fifo_path (const std::string &id)
{
    return std::string ("/tmp/musicat.") + id + ".ytdout";
}

std::string
get_ytdout_json_out_filename (const std::string &id)
{
    return std::string ("/tmp/musicat.") + id + ".ytdres.json";
}

int
has_python ()
{
    if (!system ("type python3 &> /dev/null"))
        {
            return 3;
        }
    else if (!system ("type python2 &> /dev/null"))
        {
            return 2;
        }
    else if (!system ("type python &> /dev/null"))
        {
            return 1;
        }

    return -1;
}

} // musicat::child::ytdlp
