#ifndef SHA_PLAYER_MANAGER_STREAM_H
#define SHA_PLAYER_MANAGER_STREAM_H

#include "musicat/audio_config.h"
#include <memory>

#ifdef USING_STREAM_CODEC
#elif defined(USING_LIBOPUSENC)
#include "opusenc.h"
#else
#include "opus/opus.h"
#endif // USING_LIBOPUSENC

namespace musicat::player::manager
{

#ifndef SHA_PLAYER_H
#warning Missing #include "musicat/player.h"
class guild_player_t;
#endif // SHA_PLAYER_H

#ifndef MUSICAT_DECODER_H
#warning Missing #include "musicat/decoder.h"
class decoder_t;
#endif // MUSICAT_DECODER_H

#ifndef MUSICAT_MCTRACK_H
#warning Missing #include "musicat/mctrack.h"
struct MCTrack;
#endif // MUSICAT_MCTRACK_H

void check_stream_contexts ();

void spawn_stream_thread (int count);
void stream_shutdown ();

// ================================================================================

// main loop routine
void check_embed_op_queue ();

// main loop routine
void check_download_queue ();

} // namespace musicat::player::manager

#endif // SHA_PLAYER_MANAGER_STREAM_H
