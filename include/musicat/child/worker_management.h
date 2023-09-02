#ifndef MUSICAT_CHILD_WORKER_MANAGEMENT_H
#define MUSICAT_CHILD_WORKER_MANAGEMENT_H

#include "musicat/child/worker.h"

namespace musicat
{
namespace child
{
namespace worker_management
{

int shutdown_audio_processor (worker::command_options_t &options);

} // worker_management
} // child
} // musicat

#endif // MUSICAT_CHILD_WORKER_MANAGEMENT_H
