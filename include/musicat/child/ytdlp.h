#ifndef MUSICAT_CHILD_YTDLP_H
#define MUSICAT_CHILD_YTDLP_H

#include <string>

namespace musicat::child::ytdlp
{

std::string get_ytdout_fifo_path (const std::string &id);

std::string get_ytdout_json_out_filename (const std::string &id);

int has_python ();

} // musicat::child::ytdlp

#endif // MUSICAT_CHILD_YTDLP_H
