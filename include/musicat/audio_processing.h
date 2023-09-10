#ifndef MUSICAT_AUDIO_PROCESSING_H
#define MUSICAT_AUDIO_PROCESSING_H

#include "musicat/child/command.h"
#include "musicat/player.h"
#include <fcntl.h>
#include <stdio.h>

namespace musicat
{
namespace audio_processing
{
inline constexpr size_t processing_buffer_size = BUFSIZ * 8;

struct processor_states_t
{
    // temp vars to create pipes
    int ppipefd[2];
    /* int cpipefd[2]; */
    /* int rpipefd[2]; */

    pid_t cpid;
    /* pid_t rpid; */
};

enum run_processor_error_t
{
    SUCCESS,
    ERR_INPUT,
    ERR_SFIFO,
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

// update create_options impl below when changing this struct
struct processor_options_t
{
    // static options
    std::string file_path;

    // settings
    bool debug;
    bool panic_break;
    std::string seek_to;
    int volume;
    std::string id;
    std::string guild_id;
};

processor_options_t create_options ();

processor_options_t copy_options (processor_options_t &opts);

// this should be called
// inside the streaming thread
// returns 1 if vclient terminating or null
// 0 on success
int send_audio_routine (dpp::discord_voice_client *vclient,
                        uint16_t *send_buffer, ssize_t *send_buffer_length,
                        bool no_wait = false);

// should be run as a child process
// !TODO: adjust signature to actual needed data
run_processor_error_t
run_processor (child::command::command_options_t &process_options);

std::string get_audio_stream_fifo_path (const std::string &id);

mode_t get_audio_stream_fifo_mode_t ();

std::string get_audio_stream_stdin_path (const std::string &id);

std::string get_audio_stream_stdout_path (const std::string &id);

} // audio_processing
} // musicat

#endif // MUSICAT_AUDIO_PROCESSING_H
