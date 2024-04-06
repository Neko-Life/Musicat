#ifndef MUSICAT_CHILD_WORKER_COMMAND_H
#define MUSICAT_CHILD_WORKER_COMMAND_H

#include "musicat/child/command.h"

namespace musicat::child::worker_command
{

int create_audio_processor (command::command_options_t &options);

int call_ytdlp (command::command_options_t &options);

int call_system (command::command_options_t &options);

} // musicat::child::worker_command

#endif // MUSICAT_CHILD_WORKER_COMMAND_H
