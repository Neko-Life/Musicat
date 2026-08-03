#ifndef SHA_PLAYER_MANAGER_STREAM_H
#define SHA_PLAYER_MANAGER_STREAM_H

#include "musicat/audio_config.h"
#include <memory>

#ifdef USING_LIBOPUSENC
#include "opusenc.h"
#else
#include "opus/opus.h"
#endif // USING_LIBOPUSENC

namespace musicat::player
{

#ifndef SHA_PLAYER_H
#warning Missing #include "musicat/player.h"
class Player;
#endif // SHA_PLAYER_H

#ifndef MUSICAT_DECODER_H
#warning Missing #include "musicat/decoder.h"
class decoder_t;
#endif // MUSICAT_DECODER_H

#ifndef MUSICAT_MCTRACK_H
#warning Missing #include "musicat/mctrack.h"
struct MCTrack;
#endif // MUSICAT_MCTRACK_H

struct handle_effect_chain_change_states_t
{
    std::shared_ptr<Player> &guild_player;
    MCTrack &track;
    decoder_t &dec;
};

void check_stream_contexts ();

void spawn_stream_thread (int count);
void shutdown ();

// ================================================================================

#ifdef USING_LIBOPUSENC

// this should be called inside the streaming thread
// returns 1 if vclient terminating or null
// 0 on success
int send_audio_routine (Player *guild_player, uint8_t *send_buffer, ssize_t *send_buffer_length, bool no_wait = false,
                        OggOpusEnc *opus_encoder = NULL);

#else

// this should be called inside the streaming thread
// returns 1 if vclient terminating or null
// 0 on success
int send_audio_routine (Player *guild_player, uint8_t *send_buffer, ssize_t *send_buffer_length, bool no_wait = false,
                        OpusEncoder *opus_encoder = NULL);

#endif

// main loop routine
void check_embed_op_queue ();

// main loop routine
void check_download_queue ();

} // namespace musicat::player

#endif // SHA_PLAYER_MANAGER_STREAM_H
