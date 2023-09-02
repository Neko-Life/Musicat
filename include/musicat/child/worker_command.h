#ifndef MUSICAT_CHILD_WORKER_COMMAND_H
#define MUSICAT_CHILD_WORKER_COMMAND_H

#include "musicat/child/worker.h"

namespace musicat
{
namespace child
{
namespace worker_command
{

int create_audio_processor (worker::command_options_t &options);

} // worker_command
} // child
} // musicat

#endif // MUSICAT_CHILD_WORKER_COMMAND_H
