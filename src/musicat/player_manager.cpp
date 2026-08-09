// clang-format off
#include "musicat/player.h"
#include "musicat/player_manager.h"
#include "musicat/player_manager_stream.h"
// clang-format on

#include "musicat/musicat.h"
#include "musicat/server/ws/player.h"
#include "musicat/task.h"

#include <memory>
#include <mutex>

namespace musicat::player::manager
{

using map_snowflake_guild_player = std::map<dpp::snowflake, std::shared_ptr<guild_player_t> >;
static exclusive_container<map_snowflake_guild_player> players;

static std::atomic<bool> shutdown_skip_close_voice_sessions = false;

void
handle_guild_delete (const dpp::guild_delete_t &e)
{
    auto guild_id = e.guild_id;
    if (!guild_id)
        return;

    auto lk = players.acquire ();
    auto i = players.get().find (guild_id);
    if (i == players.get().end ())
        return;
    players.get().erase (i);
}

void
set_shutdown_skip_close_voice_sessions (bool state)
{
    shutdown_skip_close_voice_sessions = state;
}

std::lock_guard<std::mutex>
acquire_players ()
{
    return players.acquire ();
}

std::map<dpp::snowflake, std::shared_ptr<guild_player_t> > *
get_players ()
{
    return &players.container;
}

std::shared_ptr<guild_player_t>
create_player (const dpp::snowflake &guild_id)
{
    auto *g = dpp::find_guild (guild_id);
    if (!g)
        return nullptr;

    auto lk = players.acquire ();
    auto l = players.get().find (guild_id);
    if (l != players.get().end ())
        return l->second;

    std::shared_ptr<guild_player_t> v = std::make_shared<guild_player_t> (guild_id, g->shard_id);
    players.get().insert (std::pair (guild_id, v));

    return v;
}

std::shared_ptr<guild_player_t>
get_player (const dpp::snowflake &guild_id)
{
    auto lk = players.acquire ();

    auto l = players.get().find (guild_id);
    if (l != players.get().end ())
        return l->second;

    return nullptr;
}

void
reconnect (const dpp::snowflake &guild_id)
{
    // this guild most likely have player present so get shard_id from guild player first
    uint32_t shard_id = 0;

    auto guild_player = get_player (guild_id);
    if (guild_player)
        shard_id = guild_player->shard_id;

    if (!shard_id)
        {
            auto *g = dpp::find_guild (guild_id);
            if (!g)
                return;
            shard_id = g->shard_id;
        }

    reconnect (shard_id, guild_id);
}

void
reconnect (dpp::discord_client *from, const dpp::snowflake &guild_id)
{
    uint32_t shard_id = from ? from->shard_id : 0;

    reconnect (shard_id, guild_id);
}

void
check_health (const dpp::snowflake &guild_id)
{
    auto *g = dpp::find_guild (guild_id);
    if (!g)
        return;
    auto *client = get_client (g->shard_id);
    if (!client)
        return;
    auto *v = client->get_voice (guild_id);
    if (!v)
        return;
    if (v->is_active ())
        return;

    if (v->is_ready ())
        v->connect ();
    else
        // connection not healthy, requires full reconnect
        full_reconnect (client, guild_id, v->channel_id, v->channel_id);
}

bool
delete_player (const dpp::snowflake &guild_id)
{
    auto lk = players.acquire ();

    auto l = players.get().find (guild_id);
    if (l == players.get().end ())
        return false;

    players.get().erase (l);
    return true;
}

std::deque<MCTrack>
get_queue (const dpp::snowflake &guild_id)
{
    auto guild_player = get_player (guild_id);
    if (!guild_player)
        return {};

    guild_player->reset_shifted ();
    return guild_player->queue;
}

bool
pause (dpp::discord_client *from, const dpp::snowflake &guild_id, const dpp::snowflake &user_id, bool _update_info_embed)
{
    auto guild_player = get_player (guild_id);
    if (!guild_player)
        return false;

    bool a = guild_player->pause (from, user_id);

    if (!a)
        return a;

    set_manually_paused (guild_id);

    if (_update_info_embed)
        update_info_embed (guild_id);

    return a;
}

void
unpause (dpp::discord_voice_client *voiceclient, const dpp::snowflake &guild_id, bool _update_info_embed)
{
    clear_manually_paused (guild_id);

    if (voiceclient)
        {
            voiceclient->pause_audio (false);
            server::ws::player::publish_play (guild_id);
        }

    if (_update_info_embed)
        update_info_embed (guild_id);
}

std::pair<std::deque<MCTrack>, int>
skip (dpp::voiceconn *v, const dpp::snowflake &guild_id, const dpp::snowflake &user_id, const int64_t &amount, const bool remove)
{
    if (!v)
        return { {}, -1 };

    auto guild_player = get_player (guild_id);
    if (!guild_player)
        return { {}, -1 };

    auto lk = guild_player->acquire ();

    guild_player->reset_shifted ();
    auto u = get_voice_from_gid (guild_id, user_id);
    if (!u.first)
        throw exception ("You're not in a voice channel", 1);

    if (u.first->id != v->channel_id)
        throw exception ("You're not in my voice channel", 0);

    // some vote logic here but decided to disable it
    // cuz i remember someone told me it's incovenient
    // also some logic is not handled properly

    // unsigned siz = 0;
    // for (auto &i : u.second)
    //     {
    //         auto &a = i.second;
    //         if (a.is_deaf () || a.is_self_deaf ())
    //             continue;
    //         auto user = dpp::find_user (a.user_id);
    //         if (user->is_bot ())
    //             continue;
    //         siz++;
    //     }

    // if (siz > 1U)
    // {
    //     std::lock_guard lk(guild_player->q_m);
    //     auto& track = guild_player->queue.at(0);
    //     auto& track = guild_player->current_track;
    //     if (track.user_id != user_id && track.user_id !=
    //     this->sha_id)
    //     {
    //         amount = 1;
    //         bool exist = false;
    //         for (const auto& i : track.skip_vote)
    //         {
    //             if (i == user_id)
    //             {
    //                 exist = true;
    //                 break;
    //             }
    //         }
    //         if (!exist)
    //         {
    //             track.skip_vote.push_back(user_id);
    //         }

    //         unsigned ts = siz / 2U + 1U;
    //         size_t ret = track.skip_vote.size();
    //         if (ret < ts) return (int)ret;
    //         else track.skip_vote.clear();
    //     }
    //     else if (amount > 1)
    //     {
    //         int64_t count = 0;
    //         for (const auto& t : guild_player->queue)
    //         {
    //             if (t.user_id == user_id || t.user_id ==
    //             this->sha_id) count++; else break;

    //             if (amount == count) break;
    //         }
    //         if (amount > count) amount = count;
    //     }
    // }

    auto removed_tracks = guild_player->skip_queue (amount, remove);
    auto [_, status] = guild_player->skip_playback (v);

    return { removed_tracks, status };
}

int
play (const dpp::snowflake &guild_id)
{
    task::run (
        [guild_id] ()
            {
                auto *cluster = get_cluster_ptr ();
                if (!cluster)
                    return;

                auto guild_player = get_player (guild_id);
                if (!guild_player)
                    {
                        std::cerr << "[player::manager::play ERROR] Guild player missing: " << guild_id << "\n";
                        return;
                    }

                bool debug = get_debug_state ();

                auto vclient = guild_player->get_voice_client ();
                if (!vclient)
                    {
                        std::cerr << "[player::manager::play ERROR] Voice client missing: " << guild_id << "\n";
                        return;
                    }

                auto lk = guild_player->acquire ();
                // text channel to send now playing embed
                dpp::snowflake channel_id = guild_player->text_channel_id;
                dpp::snowflake voice_channel_id = vclient->channel_id;

                if (debug)
                    std::cerr << "[player::manager::play] Attempt to stream: " << guild_id << ' ' << voice_channel_id << '\n';

                int err = 0;
                try
                    {
                        if (guild_player->init_for_stream () != 0)
                            return;

                        submit_stream_ctx (guild_player->guild_id);
                    }
                catch (int e)
                    {
                        err = e;

                        fprintf (stderr,
                                 "[ERROR player::manager::play] Stream thrown "
                                 "error with "
                                 "code: %d\n",
                                 e);

                        const bool has_send_msg_perm = guild_id && voice_channel_id
                                                       && has_permissions_from_ids (guild_id, cluster->me.id, channel_id,
                                                                                    { dpp::p_view_channel, dpp::p_send_messages });

                        if (!has_send_msg_perm)
                            goto skip_send_msg;

                        std::string msg = "";

                        // Maybe connect/reconnect here if there's
                        // connection error
                        if (e == 2)
                            msg = "`[ERROR]` Error while streaming, can't start "
                                  "playback";
                        else if (e == 1)
                            msg = "`[ERROR]` No connection";

                        if (!msg.empty ())
                            {
                                const dpp::message m (channel_id, msg);

                                cluster->message_create (m);
                            }
                    }

            skip_send_msg:
                return;
            });

    return 0;
}

size_t
remove_track (const dpp::snowflake &guild_id, const size_t &pos, const size_t &amount, const size_t &to)
{
    auto guild_player = get_player (guild_id);
    if (!guild_player)
        return 0;

    return guild_player->remove_track (pos, amount, to);
}

bool
shuffle_queue (const dpp::snowflake &guild_id, bool _update_info_embed)
{
    auto guild_player = get_player (guild_id);
    if (!guild_player)
        return false;

    return guild_player->shuffle (_update_info_embed);
}

dpp::discord_client *
get_client (uint32_t shard_id)
{
    auto *cluster = get_cluster_ptr ();
    if (!cluster)
        return nullptr;

    return cluster->get_shard (shard_id);
}

void
shutdown ()
{
    stream_shutdown ();

    auto *cluster = get_cluster_ptr ();
    if (!cluster)
        return;

    const auto &shards = cluster->get_shards ();
    std::vector<std::pair<uint32_t, dpp::snowflake> > shard_guilds;
    for (auto &s : shards)
        {
            std::lock_guard lk (s.second->voice_mutex);
            for (auto &p : s.second->connecting_voice_channels)
                shard_guilds.push_back ({ s.first, p.second->guild_id });
        }

    for (auto &[sid, gid] : shard_guilds)
        {
            if (shutdown_skip_close_voice_sessions)
                break;

            auto *s = get_client (sid);
            if (!s)
                continue;

            fprintf (stderr, "[player::manager::shutdown] Shard %u: Leaving voice session in guild (%ld)...\n", sid, (uint64_t)gid);

            set_disconnecting (gid, 0);
            disconnect_voice (s, gid, true);
            wait_for_disconnecting (gid);
        }
}

} // musicat::player::manager
