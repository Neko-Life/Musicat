#include "musicat/child/dl_music.h"

namespace musicat::child::dl_music
{

std::string
get_download_music_fifo_path (const std::string &id)
{
    return std::string ("/tmp/musicat.") + id + ".dlnotif";
}

} // musicat::child::dl_music
