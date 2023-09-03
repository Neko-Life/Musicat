#ifndef MUSICAT_CHILD_WORKER_COMMAND_H
#define MUSICAT_CHILD_WORKER_COMMAND_H

#include "musicat/child/command.h"

namespace musicat
{
namespace child
{
namespace worker_command
{

int create_audio_processor (command::command_options_t &options);

} // worker_command
} // child
} // musicat

#endif // MUSICAT_CHILD_WORKER_COMMAND_H
