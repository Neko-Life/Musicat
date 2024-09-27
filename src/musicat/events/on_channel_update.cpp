#include "musicat/events/on_channel_update.h"
#include "musicat/events.h"
#include "musicat/musicat.h"
#include "musicat/player.h"

namespace musicat::events
{
void
on_channel_update (dpp::cluster *client)
{
    client->on_channel_update ([] (const dpp::channel_update_t &event) {
        player::player_manager_ptr_t player_manager
            = get_player_manager_ptr ();

        if (!event.updated || !player_manager)
            return;

        const bool debug = get_debug_state ();

        const dpp::snowflake guild_id = event.updated->guild_id;
        const dpp::snowflake channel_id = event.updated->id;

        dpp::voiceconn *v = event.from->get_voice (guild_id);
        dpp::channel *cached = nullptr;
        std::shared_ptr<player::Player> guild_player;

        // reconnect if has vc and different vc region
        if (!v || !v->channel_id || (event.updated->id != v->channel_id))
            goto on_channel_update_end;

        cached = vcs_setting_get_cache (event.updated->id).first;

        // skip to end if region not changed
        if (!cached || (cached->rtc_region == event.updated->rtc_region))
            goto on_channel_update_end;

        // set to seek to last position in the next playing
        // track
        // !TODO: probably add this to player manager for
        // convenience?
        guild_player = player_manager->get_player (guild_id);

        // skip seeking when no player or no track
        if (!guild_player || !guild_player->queue.size ())
            goto on_channel_update_skip_to_rejoin;

        guild_player->check_for_to_seek();

    on_channel_update_skip_to_rejoin:
        // rejoin channel
        if (debug)
            std::cerr << "[update_rtc_region] " << cached->id << '\n';

#ifdef USE_VOICE_SERVER_UPDATE_RECONNECT
        // skip this reconnect to use new voice server update handler
        goto on_channel_update_end;
#endif

        player_manager->full_reconnect (event.from, guild_id, channel_id,
                                        channel_id);

    on_channel_update_end:
        // update vc cache
        vcs_setting_handle_updated (event.updated, nullptr);
    });
}
} // musicat::events
