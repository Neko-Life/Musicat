#ifndef SHA_PLAYER_MANAGER_STREAM_H
#define SHA_PLAYER_MANAGER_STREAM_H

namespace musicat::player::manager
{

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
