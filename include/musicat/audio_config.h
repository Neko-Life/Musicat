#ifndef MUSICAT_AUDIO_CONFIG_H
#define MUSICAT_AUDIO_CONFIG_H

#include "musicat/config.h"
#include <cstddef>
#include <cstdio>
#include <opus/opus_types.h>

#define SEND_DURATION 20
#define ENCODE_BUFFER_SIZE opus_encode_buffer_size
#define FRAME_SIZE opus_frame_size
#define MAX_PACKET 4096

inline constexpr const size_t opus_frame_size = SEND_DURATION * 48;
// FRAME_SIZE * channel * sizeof (opus_int16)
inline constexpr const size_t opus_encode_buffer_size
    = FRAME_SIZE * 2 * sizeof (opus_int16);

#ifdef MUSICAT_USE_PCM

#define STREAM_BUFSIZ ENCODE_BUFFER_SIZE
#define CHUNK_READ CHUNK_READ_PCM
#define DRAIN_CHUNK DRAIN_CHUNK_PCM

#else

#include <oggz/oggz.h>

#define STREAM_BUFSIZ STREAM_BUFSIZ_OPUS
#define CHUNK_READ CHUNK_READ_OPUS
#define DRAIN_CHUNK DRAIN_CHUNK_OPUS

#endif

// correct frame size with timescale for dpp
#define STREAM_BUFSIZ_PCM dpp::send_audio_raw_max_length
inline constexpr long CHUNK_READ_PCM = BUFSIZ * 2;
inline constexpr long DRAIN_CHUNK_PCM = BUFSIZ / 2;

inline constexpr long STREAM_BUFSIZ_OPUS = BUFSIZ / 8;
inline constexpr long CHUNK_READ_OPUS = BUFSIZ / 2;
inline constexpr long DRAIN_CHUNK_OPUS = BUFSIZ / 4;

#endif // MUSICAT_AUDIO_CONFIG_H
