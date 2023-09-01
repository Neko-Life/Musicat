#include "musicat/audio_processing.h"
#include "musicat/child/worker.h"
#include <unistd.h>

namespace musicat
{
namespace child
{
namespace worker_command
{

int
create_audio_processor (worker::worker_command_options_t &options)
{
    pid_t status = fork();

    if (status < 0) return status;

    if (status == 0) {
        status = audio_processing::run_processor(options.file_path, options.debug);
        _exit(status);
    }

    return 0;
}

} // worker_command
} // child
} // musicat
