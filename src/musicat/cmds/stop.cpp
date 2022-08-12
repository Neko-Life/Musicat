#include "musicat/cmds.h"

namespace musicat {
    namespace command {
        namespace stop {
            dpp::slashcommand get_register_obj(const dpp::snowflake sha_id) {
                return dpp::slashcommand("stop", "STOP CURRENT PLAYBACK! STOP IT NOW!!!", sha_id);
            }

            void slash_run(const dpp::interaction_create_t& event, player_manager_ptr player_manager) {
                auto p = player_manager->get_player(event.command.guild_id);
                dpp::voiceconn* v = event.from->get_voice(event.command.guild_id);

                if (!p || !v
                    || !v->voiceclient
                    || !v->voiceclient->is_ready()
                    || (v->voiceclient->get_secs_remaining() < 0.1f && p && p->queue.begin() == p->queue.end()))
                {
                    event.reply("I'm not playing anything");
                    return;
                }

                if (!player_manager->voice_ready(event.command.guild_id, event.from, event.command.usr.id))
                {
                    event.reply("Please wait while I'm getting ready to stream");
                    return;
                }

                try
                {
                    auto vcu = get_voice_from_gid(event.command.guild_id, event.command.usr.id);
                    if (vcu.first->id != v->channel_id)
                    {
                        event.reply("You're not in my voice channel!");
                        return;
                    }
                }
                catch (...)
                {
                    event.reply("You're not in a voice channel!");
                    return;
                }

                player_manager->stop_stream(event.command.guild_id);
                p->skip(v);
                p->set_stopped(true);

                v->voiceclient->pause_audio(true);
                {
                    std::lock_guard<std::mutex> lk(player_manager->mp_m);
                    auto l = vector_find(&player_manager->manually_paused, event.command.guild_id);
                    if (l == player_manager->manually_paused.end())
                    {
                        player_manager->manually_paused.push_back(event.command.guild_id);
                    }
                }

                event.reply("Stopped");
            }
        }
    }
}
