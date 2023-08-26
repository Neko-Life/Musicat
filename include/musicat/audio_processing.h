#ifndef MUSICAT_AUDIO_PROCESSING_H
#define MUSICAT_AUDIO_PROCESSING_H

#include "musicat/player.h"
#include <fcntl.h>
#include <stdio.h>

namespace musicat
{
namespace audio_processing
{
static const size_t processing_buffer_size = BUFSIZ * 5;

struct parent_child_ic_t
{
    int ppipefd[2];
    int cpipefd[2];
    pid_t cpid;
};

enum run_processor_error_t
{
    SUCCESS,
    ERR_INPUT,
    ERR_SPIPE,
    ERR_SFORK,
    ERR_LPIPE,
    ERR_LFORK,
};

struct track_data_t
{
    player::MCTrack *track;
    std::shared_ptr<player::Player> player;
};

} // audio_processing
} // musicat

#endif // MUSICAT_AUDIO_PROCESSING_H
