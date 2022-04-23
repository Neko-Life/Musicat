#include <filesystem>
#include <sys/stat.h>
#include <unistd.h>
#include "musicat.h"
#include "yt-search.h"
#include "sha_player.h"

using json = nlohmann::json;
using string = std::string;

bool running = true;

void on_sigint(int code) {
    printf("RECEIVED SIGINT\nCODE: %d\n", code);
    running = false;
}

int main()
{
    {
        struct stat buf;
        if (stat("music", &buf) != 0)
            std::filesystem::create_directory("music");
    }
    json sha_cfg;
    {
        std::ifstream scs("sha_conf.json");
        scs >> sha_cfg;
        scs.close();
    }

    dpp::cluster client(sha_cfg["SHA_TKN"], dpp::i_message_content | dpp::i_guild_members | dpp::i_default_intents);

    mc::settings sha_settings;
    dpp::snowflake sha_id(sha_cfg["SHA_ID"]);
    sha_settings.defaultPrefix = sha_cfg["SHA_PREFIX"];
    printf("GLOBAL_PREFIX: %s\n", sha_settings.defaultPrefix.c_str());

    Sha_Player_Manager* player_manager = new Sha_Player_Manager(&client, sha_id);

    client.on_log(dpp::utility::cout_logger());

    client.on_ready([](const dpp::ready_t& event)
    {
        printf("SHARD: %d\nWS_PING: %f\n", event.shard_id, event.from->websocket_ping);
    });

    client.on_message_create([&player_manager, &client, &sha_settings, &sha_id](const dpp::message_create_t& event)
    {
        if (event.msg.author.is_bot()) return;

        std::smatch content_regex_results;
        {
            string cstr = string("^((?:")
                .append(sha_settings.get_prefix(event.msg.guild_id))
                .append("|<@\\!?")
                .append(std::to_string(sha_id))
                .append(">)\\s*)([^\\s]*)(\\s+)?(.*)");
            std::regex re(cstr.c_str(), std::regex_constants::ECMAScript | std::regex_constants::icase);
            auto reFlag = std::regex_constants::match_not_null | std::regex_constants::match_any;
            std::regex_search(event.msg.content, content_regex_results, re, reFlag);
        }

        if (!content_regex_results.size()) return;

        string prefix = content_regex_results[1].str();
        string cmd = content_regex_results[2].str();
        for (size_t s = 0; s < cmd.length(); s++)
        {
            cmd[s] = std::tolower(cmd[s]);
        }
        string cmd_args = content_regex_results[4].str();

        if (cmd == "hello") event.reply("Hello there!!");
        else if (cmd == "why") event.reply("Why not");
        else if (cmd == "hi") event.reply("HIII");
        else if (cmd == "invite") event.reply("https://discord.com/api/oauth2/authorize?client_id=960168583969767424&permissions=412353875008&scope=bot%20applications.commands");
        else if (cmd == "support") event.reply("https://www.discord.gg/vpk2KyKHtu");
        else if (cmd == "repo") event.reply("https://github.com/Neko-Life/Musicat");
        else if (cmd == "pause")
        {
            try
            {
                if (player_manager->pause(event.from, event.msg.guild_id, event.msg.author.id)) event.reply("Paused");
                else event.reply("I'm not playing anything");
            }
            catch (mc::exception e)
            {
                return event.reply(e.what());
            }
        }
        // else if (cmd == "resume")
        // {
        //     auto v = event.from->get_voice(event.msg.guild_id);
        //     if (v)
        //     {
        //         if (v->voiceclient->is_paused())
        //         {
        //             try
        //             {
        //                 auto u = mc::get_voice_from_gid(event.msg.guild_id, event.msg.author.id);
        //                 if (u.first->id != v->channel_id) return event.reply("You're not in my voice channel");
        //             }
        //             catch (const char* e)
        //             {
        //                 return event.reply("You're not in a voice channel");
        //             }
        //             v->voiceclient->pause_audio(false);
        //             {
        //                 std::lock_guard<std::mutex> lk(player_manager->mp_m);
        //                 auto l = mc::vector_find(&manually_paused, event.msg.guild_id);
        //                 if (l != manually_paused.end())
        //                     manually_paused.erase(l);
        //             }
        //             event.reply("Resumed");
        //         }
        //         else event.reply("I'm playing right now");
        //     }
        //     else event.reply("I'm not in any voice channel");
        // }
        // else if (cmd == "stop")
        // {
        //     auto v = event.from->get_voice(event.msg.guild_id);
        //     if (v && (v->voiceclient->is_playing() || v->voiceclient->is_paused()))
        //     {
        //         try
        //         {
        //             auto u = mc::get_voice_from_gid(event.msg.guild_id, event.msg.author.id);
        //             if (u.first->id != v->channel_id) return event.reply("You're not in my voice channel");
        //         }
        //         catch (const char* e)
        //         {
        //             return event.reply("You're not in a voice channel");
        //         }
        //         v->voiceclient->stop_audio();
        //         event.reply("Stopped");
        //     }
        //     else event.reply("I'm not playing anything right now");
        // }
        else if (cmd == "skip")
        {
            try
            {
                auto v = event.from->get_voice(event.msg.guild_id);
                if (player_manager->skip(v, event.msg.guild_id, event.msg.author.id))
                {
                    event.reply("Skipped");
                }
                else event.reply("I'm not playing anything");
            }
            catch (mc::exception e)
            {
                return event.reply(e.what());
            }
        }
        else if (cmd == "play")
        {
            auto guild_id = event.msg.guild_id;
            auto from = event.from;
            auto user_id = event.msg.author.id;

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
                from->disconnect_voice(guild_id);
                player_manager->disconnecting[guild_id] = vcclient.first->id;
            }
            if (vcclient_cont && vcclient.first->id != vcuser.first->id)
            {
                if (vcclient.second.size() > 1)
                {
                    for (auto r : vcclient.second)
                    {
                        auto u = dpp::find_user(r.second.user_id);
                        if (!u)
                        {
                            try
                            {
                                dpp::user_identified uf = client.user_get_sync(r.second.user_id);
                                if (uf.is_bot()) continue;
                            }
                            catch (const dpp::rest_exception* e)
                            {
                                printf("[ERROR(main.112)] Can't fetch user in VC: %ld\n", r.second.user_id);
                                continue;
                            }
                        }
                        else if (u->is_bot()) continue;
                        event.reply("Sorry but I'm already in another voice channel");
                        return;
                    }
                }
                // if (v)
                // {
                //     printf("Stopping audio\n");
                //     v->voiceclient->stop_audio();
                // }
                vcclient_cont = false;
                if (v && cmd_args.length() > 0)
                {
                    printf("Disconnecting as no member in vc: %ld\n", guild_id);
                    from->disconnect_voice(guild_id);
                    player_manager->disconnecting[guild_id] = vcclient.first->id;
                }
            }

            if (v && v->voiceclient && v->voiceclient->get_tracks_remaining() > 0 && v->voiceclient->is_paused() && v->channel_id == vcuser.first->id)
            {
                {
                    std::lock_guard<std::mutex> lk(player_manager->mp_m);
                    auto l = mc::vector_find(&player_manager->manually_paused, guild_id);
                    if (l != player_manager->manually_paused.end())
                        player_manager->manually_paused.erase(l);
                }
                v->voiceclient->pause_audio(false);
            }

            if (cmd_args.length() == 0) return event.reply("Provide song query if you wanna add a song, may be URL or song name");

            event.reply("Searching...");

            auto searches = yt_search(cmd_args).trackResults();
            if (!searches.size()) return event.reply("Can't find anything");
            auto result = searches.front();
            string fname = std::regex_replace(result.title() + string("-") + result.id() + string(".ogg"), std::regex("/"), "", std::regex_constants::match_any);
            event.reply(string("Added: ") + result.title());

            if (vcclient_cont == false || !v)
            {
                event.from->connect_voice(event.msg.guild_id, vcuser.first->id);
                player_manager->connecting[guild_id] = vcuser.first->id;
                player_manager->waiting_vc_ready[guild_id] = fname;
            }

            bool dling = false;

            std::ifstream test((string("music/") + fname).c_str());
            if (!test.is_open())
            {
                dling = true;
                event.reply("Downloading... Gimme 10 sec ight");
                if (player_manager->waiting_file_download.find(fname) == player_manager->waiting_file_download.end())
                {
                    auto url = result.url();
                    player_manager->download(fname, url, guild_id);
                }
            }
            else test.close();

            if (dling)
            {
                std::unique_lock<std::mutex> lk(player_manager->dl_m);
                player_manager->dl_cv.wait(lk, [&player_manager, fname]() {
                    return player_manager->waiting_file_download.find(fname) == player_manager->waiting_file_download.end();
                });
            }
            auto p = player_manager->create_player(event.msg.guild_id);
            Sha_Track t(result);
            t.filename = fname;
            t.user_id = event.msg.author.id;
            p->add_track(t);
            if (v && v->voiceclient && v->voiceclient->get_tracks_remaining() == 0) v->voiceclient->insert_marker();
        }
    });

    client.on_voice_ready([&player_manager](const dpp::voice_ready_t& event) {
        player_manager->handle_on_voice_ready(event);
    });

    /* yt-dlp -f 251 'https://www.youtube.com/watch?v=FcRJGHkpm8s' -o - | ffmpeg -i - -ar 48000 -ac 2 -sn -dn -c [opus|libopus|copy] -f opus - */
    client.on_voice_state_update([&player_manager](const dpp::voice_state_update_t& event) {
        player_manager->handle_on_voice_state_update(event);
    });

    client.on_voice_track_marker([&player_manager](const dpp::voice_track_marker_t& event) {
        // {
        //     std::lock_guard<std::mutex> lk(wm_m);
        //     printf("on_voice_track_marker\n");
        //     std::vector<std::string> l = waiting_marker.find(event.voice_client->server_id)->second;
        //     for (int i = 0; i < (int)(l.size()); i++)
        //     {
        //         printf("%s | %s\n", l.at(i).c_str(), event.track_meta.c_str());
        //         if (l.at(i) == event.track_meta)
        //             l.erase(l.begin() + i);
        //     }
        //     dl_cv.notify_all();
        // }
        player_manager->handle_on_track_marker(event);
    });

    // client.set_websocket_protocol(dpp::websocket_protocol_t::ws_etf);

    client.start(true);

    signal(SIGINT, on_sigint);

    while (running) sleep(1);

    delete player_manager;

    return 0;
}
