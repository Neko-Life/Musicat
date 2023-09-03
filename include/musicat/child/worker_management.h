#ifndef MUSICAT_CHILD_WORKER_MANAGEMENT_H
#define MUSICAT_CHILD_WORKER_MANAGEMENT_H

#include "musicat/child/command.h"

namespace musicat
{
namespace child
{
namespace worker_management
{

int shutdown_audio_processor (command::command_options_t &options);

int clean_up_audio_processor (command::command_options_t &options);

} // worker_management
} // child
} // musicat

#endif // MUSICAT_CHILD_WORKER_MANAGEMENT_H
