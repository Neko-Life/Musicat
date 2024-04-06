#ifndef MUSICAT_AUDIO_PROCESSING_H
#define MUSICAT_AUDIO_PROCESSING_H

#include "musicat/child/command.h"
#include "musicat/player.h"
#include <fcntl.h>
#include <memory>
#include <stdio.h>

#ifdef MUSICAT_USE_PCM

#define USING_FORMAT FORMAT_USING_PCM
#define BUFFER_SIZE PROCESSING_BUFFER_SIZE_PCM
#define READ_CHUNK_SIZE READ_CHUNK_SIZE_PCM

#else

#define USING_FORMAT FORMAT_USING_OPUS
#define BUFFER_SIZE PROCESSING_BUFFER_SIZE_OPUS
#define READ_CHUNK_SIZE READ_CHUNK_SIZE_OPUS

#endif

#define FORMAT_USING_OPUS "-f", "opus", "-b:a", "128k"
#define FORMAT_USING_PCM "-f", "s16le"

#define OUT_CMD "pipe:1"
#define FFMPEG_REALTIME

inline constexpr ssize_t READ_CHUNK_SIZE_PCM = BUFSIZ / 2;
inline constexpr ssize_t READ_CHUNK_SIZE_OPUS = BUFSIZ / 8;

// inline constexpr size_t PROCESSING_BUFFER_SIZE_PCM = BUFSIZ * 8;
inline constexpr size_t PROCESSING_BUFFER_SIZE_PCM = BUFSIZ / 2;

inline constexpr size_t PROCESSING_BUFFER_SIZE_OPUS = BUFSIZ / 2;

namespace musicat
{
namespace audio_processing
{
struct processor_states_t
{
    // temp vars to create pipes
    int ppipefd[2];
    int cpipefd[2];
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

// be sure to update parse_helper_chain_option below
// when updating this
struct helper_chain_option_t
{
    bool debug;
    std::string raw_args;
    bool parsed;
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

    // chain of effects
    std::deque<helper_chain_option_t> helper_chain;
};

processor_options_t create_options ();

processor_options_t copy_options (processor_options_t &opts);

/*
@hello@@t@
0123456789
0+1,6;7+1,9
1,6-1;8,9-8
1, 5 ;8, 1
*/

static inline int
parse_helper_chain_option (
    const child::command::command_options_t &command_options,
    processor_options_t &processor_options)
{
    size_t co_length = command_options.helper_chain.length ();
    size_t start_d = 0;
    for (size_t i = 0; i < co_length; i++)
        {
            if (command_options.helper_chain[i] != '@')
                continue;

            if (start_d == 0)
                {
                    start_d = i + 1;
                    continue;
                }

            int64_t res = i - start_d;

            if (res <= 0)
                {
                    start_d = 0;
                    continue;
                }

            processor_options.helper_chain.push_back (
                { command_options.debug,
                  command_options.helper_chain.substr (start_d, res), false });

            start_d = 0;
        }

    return 0;
}

ssize_t write_stdout (uint8_t *buffer, ssize_t *size,
                      bool no_effect_chain = false);

// this should be called inside the streaming thread
// returns 1 if vclient terminating or null
// 0 on success
int send_audio_routine (dpp::discord_voice_client *vclient,
                        uint16_t *send_buffer, ssize_t *send_buffer_length,
                        bool no_wait = false);

// should be run as a child process
run_processor_error_t
run_processor (child::command::command_options_t &process_options);

std::string get_audio_stream_fifo_path (const std::string &id);

mode_t get_audio_stream_fifo_mode_t ();

std::string get_audio_stream_stdin_path (const std::string &id);

std::string get_audio_stream_stdout_path (const std::string &id);

} // audio_processing
} // musicat

#endif // MUSICAT_AUDIO_PROCESSING_H
