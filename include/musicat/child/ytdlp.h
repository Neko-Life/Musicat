#ifndef MUSICAT_CHILD_YTDLP_H
#define MUSICAT_CHILD_YTDLP_H

#include "musicat/child/command.h"
#include <semaphore.h>
#include <string>

namespace musicat::child::ytdlp
{

std::string get_ytdout_fifo_path (const std::string &id);

std::string get_ytdout_json_out_filename (const std::string &id);

int has_python ();

int run (const command::command_options_t &options, sem_t *sem,
         const std::string &sem_full_key);

} // musicat::child::ytdlp

#endif // MUSICAT_CHILD_YTDLP_H
