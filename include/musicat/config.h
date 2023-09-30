#ifndef MUSICAT_CONFIG_H
#define MUSICAT_CONFIG_H

// !TODO: have an adjustable value based on active effect chain
#define DPP_AUDIO_BUFFER_LENGTH_SECOND 1.0f
#define SLEEP_ON_BUFFER_THRESHOLD_MS 50

// use etf websocket protocol
// #define MUSICAT_WS_P_ETF

// use pcm instead of opus stream audio format
#define MUSICAT_USE_PCM

#endif // MUSICAT_CONFIG_H
