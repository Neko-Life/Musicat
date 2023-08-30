#ifndef MUSICAT_AUDIO_PROCESSING_H
#define MUSICAT_AUDIO_PROCESSING_H

#include "musicat/player.h"
#include <fcntl.h>
#include <stdio.h>

namespace musicat
{
namespace audio_processing
{
static const size_t processing_buffer_size = BUFSIZ * 8;

struct parent_child_ic_t
{
    int ppipefd[2];
    int cpipefd[2];
    int rpipefd[2];
    pid_t cpid;
    pid_t rpid;
    bool debug;
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
    std::string file_path;
    std::shared_ptr<player::Player> player;
    dpp::discord_voice_client *vclient;
};

// this should be called
// inside the streaming thread
// returns 1 if vclient terminating or null
// 0 on success
static int send_audio_routine (dpp::discord_voice_client *vclient,
                               uint16_t *send_buffer,
                               size_t *send_buffer_length,
                               bool no_wait = false);

// should be run as a child process
// !TODO: adjust signature to actual needed data
run_processor_error_t run_processor (track_data_t *p_track,
                                     parent_child_ic_t *p_info);

} // audio_processing
} // musicat

#endif // MUSICAT_AUDIO_PROCESSING_H
