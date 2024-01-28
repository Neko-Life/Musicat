#include "musicat/child/ytdlp.h"

namespace musicat::child::ytdlp
{

std::string
get_ytdout_fifo_path (const std::string &id)
{
    return std::string ("/tmp/musicat.") + id + ".ytdout";
}

} // musicat::child::ytdlp
