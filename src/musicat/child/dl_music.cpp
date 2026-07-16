#include "musicat/child/dl_music.h"
#include "musicat/child.h"

namespace musicat::child::dl_music
{

std::string
get_download_music_fifo_path (const std::string &id)
{
    return get_local_dir_ensure () + id + ".dlnotif";
}

} // musicat::child::dl_music
