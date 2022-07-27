#include <regex>
#include <vector>
#include "musicat/musicat.h"
#include "musicat/cmds.h"
#include "musicat/yt-search.h"
#include "musicat/yt-playlist.h"

namespace musicat_command {
    namespace play {
        namespace autocomplete {
            void query(const dpp::autocomplete_t& event, string param, player_manager_ptr player_manager, dpp::cluster& client) {
                std::vector<string> avail = {};
                if (!param.length()) avail = player_manager->get_available_tracks(25);
                else
                {
                    auto r = player_manager->get_available_tracks();
                    for (auto i : r)
                    {
                        const auto ori = i;
                        for (auto& j : i) j = std::tolower(j);
                        for (auto& j : param) j = std::tolower(j);
                        if (i.find(param) != string::npos)
                            avail.push_back(ori);
                        if (avail.size() == 25) break;
                    }
                }
                dpp::interaction_response r(dpp::ir_autocomplete_reply);
                for (const auto& i : avail)
                {
                    auto v = i.length() > 100 ? i.substr(0, 100) : i;
                    r.add_autocomplete_choice(dpp::command_option_choice(v, v));
                }
                client.interaction_response_create(event.command.id, event.command.token, r);
            }
        }

        dpp::slashcommand get_register_obj(const dpp::snowflake sha_id) {
            return dpp::slashcommand(
                "play",
                "Play [a song], resume [paused playback], or add [song to queue]",
                sha_id
            ).add_option(
                dpp::command_option(
                    dpp::co_string,
                    "query",
                    "Song [to search] or Youtube URL [to play]"
                ).set_auto_complete(true)
            ).add_option(
                dpp::command_option(
                    dpp::co_integer,
                    "top",
                    "Add [this song] to the top [of the queue]"
                ).add_choice(
                    dpp::command_option_choice("Yes", 1)
                ).add_choice(
                    dpp::command_option_choice("No", 0)
                )
            );
        }

        void slash_run(const dpp::interaction_create_t& event, player_manager_ptr player_manager) {
            if (!player_manager->voice_ready(event.command.guild_id, event.from, event.command.usr.id))
            {
                event.reply("Please wait while I'm getting ready to stream");
                return;
            }

            auto guild_id = event.command.guild_id;
            auto from = event.from;
            auto user_id = event.command.usr.id;
            const dpp::snowflake sha_id = player_manager->sha_id;
            string arg_query = "";
            int64_t arg_top = 0;
            mc::get_inter_param(event, "query", &arg_query);
            mc::get_inter_param(event, "top", &arg_top);

            dpp::guild* g = dpp::find_guild(guild_id);
            std::pair<dpp::channel*, std::map<dpp::snowflake, dpp::voicestate>> vcuser;
            try
            {
                vcuser = mc::get_voice(g, user_id);
            }
            catch (const char* e)
            {
                event.reply("Join a voice channel first you dummy");
                return;
            }

            dpp::user* sha_user = dpp::find_user(sha_id);

            uint64_t cperm = g->permission_overwrites(g->base_permissions(sha_user), sha_user, vcuser.first);
            printf("c: %ld\npv: %ld\npp: %ld\n", cperm, cperm & dpp::p_view_channel, cperm & dpp::p_connect);

            if (!(cperm & dpp::p_view_channel && cperm & dpp::p_connect))
            {
                event.reply("I have no permission to join your voice channel");
                return;
            }

            std::pair<dpp::channel*, std::map<dpp::snowflake, dpp::voicestate>> vcclient;
            // Whether client in vc and vcclient exist
            bool vcclient_cont = true;
            try { vcclient = mc::get_voice(g, sha_id); }
            catch (const char* e)
            {
                vcclient_cont = false;
                if (from->connecting_voice_channels.find(guild_id) != from->connecting_voice_channels.end())
                {
                    printf("Disconnecting as not in vc but connected state still in cache: %ld\n", guild_id);
                    from->disconnect_voice(guild_id);
                }
            }

            // Client voice conn
            dpp::voiceconn* v = from->get_voice(guild_id);
            if (vcclient_cont && v && v->channel_id != vcclient.first->id)
            {
                vcclient_cont = false;
                printf("Disconnecting as it seems I just got moved to different vc and connection not updated yet: %ld\n", guild_id);
                std::lock_guard<std::mutex> lk(player_manager->dc_m);
                player_manager->disconnecting[guild_id] = vcclient.first->id;
                from->disconnect_voice(guild_id);
            }
            if (vcclient_cont && vcclient.first->id != vcuser.first->id)
            {
                if (mc::has_listener(&vcclient.second))
                    return event.reply("Sorry but I'm already in another voice channel");
                // if (v)
                // {
                //     printf("Stopping audio\n");
                //     v->voiceclient->stop_audio();
                // }
                vcclient_cont = false;
                if (v)
                {
                    printf("Disconnecting as no member in vc: %ld connecting to %ld\n", guild_id, vcuser.first->id);
                    if (v && v->voiceclient && v->voiceclient->get_secs_remaining() > 0.1)
                    {
                        if (!v->voiceclient->terminating)
                        {
                            v->voiceclient->pause_audio(false);
                            v->voiceclient->skip_to_next_marker();
                        }
                        player_manager->stop_stream(guild_id);
                    }

                    {
                        std::lock_guard<std::mutex> lk(player_manager->dc_m);
                        player_manager->disconnecting[guild_id] = vcclient.first->id;
                        std::lock_guard<std::mutex> lk2(player_manager->c_m);
                        std::lock_guard<std::mutex> lk3(player_manager->wd_m);
                        player_manager->connecting.insert_or_assign(guild_id, vcuser.first->id);
                        player_manager->waiting_vc_ready[guild_id] = "0";
                        from->disconnect_voice(guild_id);
                    }

                    std::thread pjt([player_manager, from, guild_id]() {
                        player_manager->reconnect(from, guild_id);
                    });
                    pjt.detach();
                }
            }

            if (v && v->voiceclient && v->voiceclient->is_paused() && v->channel_id == vcuser.first->id)
            {
                player_manager->unpause(v->voiceclient, event.command.guild_id);
                if (arg_query.length() == 0) return event.reply("Resumed");
            }

            auto op = player_manager->get_player(event.command.guild_id);
            bool continued = false;
            if (op)
            {
                std::lock_guard<std::mutex> lk(op->q_m);
                if (v
                    && v->voiceclient
                    && op->queue.size()
                    && !v->voiceclient->is_paused()
                    && !v->voiceclient->is_playing())
                {
                    v->voiceclient->insert_marker("c");
                    continued = true;
                }
            }

            if (arg_query.length() == 0) return event.reply("Provide song query if you wanna add a song, may be URL or song name");

            event.thinking();
            add_track(false, guild_id, arg_query, arg_top, vcclient_cont, v, vcuser.first->id, sha_id, player_manager, true, from, event, continued);
        }

        void add_track(bool playlist,
            dpp::snowflake guild_id,
            string arg_query,
            int64_t arg_top,
            bool vcclient_cont,
            dpp::voiceconn* v,
            const dpp::snowflake channel_id,
            const dpp::snowflake sha_id,
            player_manager_ptr player_manager,
            bool from_interaction,
            dpp::discord_client* from,
            const dpp::interaction_create_t event,
            bool continued) {
            std::vector<yt_search::YTrack> searches = playlist ? yt_search::get_playlist(arg_query).entries() : yt_search::search(arg_query).trackResults();
            if (!searches.size())
            {
                if (from_interaction) event.edit_response("Can't find anything");
                return;
            }
            yt_search::YTrack result = {};
            if (playlist == false) result = searches.front();
            else
            {
                auto p = player_manager->get_player(guild_id);
                if (!p) return;
                std::lock_guard<std::mutex> lk(p->q_m);
                if (!p->queue.size()) return;
                for (auto i : searches)
                {
                    auto iid = i.id();
                    bool br = false;
                    for (auto& a : p->queue)
                    {
                        if (a.id() == iid)
                        {
                            br = true;
                            break;
                        }
                    }
                    if (!br)
                    {
                        std::lock_guard<std::mutex> lk(p->h_m);
                        if (p->history.size()) for (const auto& a : p->history)
                        {
                            if (a == iid)
                            {
                                br = true;
                                break;
                            }
                        }
                    }
                    if (br) continue;
                    result = i;
                    break;
                }
                if (result.raw.is_null()) return;
            }
            string fname = std::regex_replace(result.title() + string("-") + result.id() + string(".ogg"), std::regex("/"), "", std::regex_constants::match_any);

            if (from_interaction && (vcclient_cont == false || !v))
            {
                std::lock_guard<std::mutex> lk2(player_manager->c_m);
                std::lock_guard<std::mutex> lk3(player_manager->wd_m);
                player_manager->connecting[guild_id] = channel_id;
                printf("INSERTING WAIT FOR VC READY\n");
                player_manager->waiting_vc_ready[guild_id] = fname;
            }

            bool dling = false;

            std::ifstream test((string("music/") + fname).c_str());
            if (!test.is_open())
            {
                dling = true;
                if (from_interaction) event.edit_response(string("Downloading ") + result.title() + string("... Gimme 10 sec ight"));
                if (player_manager->waiting_file_download.find(fname) == player_manager->waiting_file_download.end())
                {
                    auto url = result.url();
                    player_manager->download(fname, url, guild_id);
                }
            }
            else
            {
                test.close();
                if (from_interaction) event.edit_response(string("Added: ") + result.title());
            }

            std::thread pjt([player_manager, from, guild_id]() {
                player_manager->reconnect(from, guild_id);
            });
            pjt.detach();

            std::thread dlt([player_manager, sha_id, dling, fname, arg_top, from_interaction, guild_id, from, continued](const dpp::interaction_create_t event, yt_search::YTrack result) {
                dpp::snowflake user_id = from_interaction ? event.command.usr.id : sha_id;
                auto p = player_manager->create_player(guild_id);
                dpp::snowflake channel_id = from_interaction ? event.command.channel_id : p->channel_id;
                if (dling)
                {
                    player_manager->wait_for_download(fname);
                    if (from_interaction) event.edit_response(string("Added: ") + result.title());
                }
                if (from) p->from = from;

                mpl::MCTrack t(result);
                t.filename = fname;
                t.user_id = user_id;
                p->add_track(t, arg_top ? true : false, guild_id, from_interaction || dling);
                if (from_interaction) p->set_channel(channel_id);

                std::pair<dpp::channel*, std::map<dpp::snowflake, dpp::voicestate>> vu;
                bool b = true;
                try { vu = mc::get_voice_from_gid(guild_id, sha_id); }
                catch (const char* e) { b = false; }

                if (from_interaction && !continued && from)
                {
                    dpp::voiceconn* v = from->get_voice(guild_id);
                    if (b && mc::has_listener(&vu.second)
                        && v && v->voiceclient && v->voiceclient->is_ready() && v->voiceclient->get_secs_remaining() < 0.1)
                        v->voiceclient->insert_marker("s");
                }
            }, event, result);
            dlt.detach();
        }
    }
}
