#ifndef MUSICAT_CONFIG_H
#define MUSICAT_CONFIG_H

#define SLEEP_ON_BUFFER_THRESHOLD_MS 60

// use etf websocket protocol
// #define MUSICAT_WS_P_ETF

// use pcm instead of opus stream audio format
// opus doesnt support filters currently
// opus also add significant processing delay
#define MUSICAT_USE_PCM

// add exciter effect to ffmpeg input stream
// make the audio more exciting!
#define AUDIO_INPUT_USE_EXCITER

#endif // MUSICAT_CONFIG_H
