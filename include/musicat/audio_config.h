#ifndef MUSICAT_AUDIO_CONFIG_H
#define MUSICAT_AUDIO_CONFIG_H

#include "musicat/config.h"
#include <cstddef>
#include <cstdio>
#include <opus/opus_types.h>

#define FRAME_DURATION 40
#define ENCODE_BUFFER_SIZE opus_encode_buffer_size
#define FRAME_SIZE opus_frame_size

// this is the recommended value for encoded opus output buffer size
// https://www.opus-codec.org/docs/html_api/group__opusencoder.html
#define OPUS_MAX_ENCODE_OUTPUT_SIZE 1276

inline constexpr const size_t opus_frame_size = FRAME_DURATION * 48;
// FRAME_SIZE * channel * sizeof (opus_int16)
inline constexpr const size_t opus_encode_buffer_size
    = FRAME_SIZE * 2 * sizeof (opus_int16);

#define STREAM_BUFSIZ ENCODE_BUFFER_SIZE
#define DRAIN_CHUNK DRAIN_CHUNK_PCM

inline constexpr long DRAIN_CHUNK_PCM = BUFSIZ / 2;

#endif // MUSICAT_AUDIO_CONFIG_H
