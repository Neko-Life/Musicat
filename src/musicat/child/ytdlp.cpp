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

} // musicat::child::ytdlp
