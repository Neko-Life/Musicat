#ifndef MUSICAT_CHILD_SYSTEM_H
#define MUSICAT_CHILD_SYSTEM_H

#include "musicat/child/command.h"
#include <semaphore.h>
#include <string>

namespace musicat::child::system
{

std::string get_system_fifo_path (const std::string &id);

int run (const command::command_options_t &options, sem_t *sem,
         const std::string &sem_full_key);

} // musicat::child::system

#endif // MUSICAT_CHILD_YTDLP_H
