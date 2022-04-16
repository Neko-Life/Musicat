#include <filesystem>
#include <sys/stat.h>
#include "musicat.h"
#include "yt-search.h"

using json = nlohmann::json;
using string = std::string;
// const dpp::snowflake SHA_GID = 823176176641376296;

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

    // Mutexes
    // dl: waiting_file_download
    // wd: waiting_vc_ready
    // c: connecting
    // dc: disconnecting
    // wm: waiting_marker
    // mp: manually_paused
    std::mutex dl_m, wd_m, c_m, dc_m, wm_m, mp_m;
    // Conditional variable, use notify_all
    std::condition_variable dl_cv;

    std::map<uint64_t, uint64_t> connecting, disconnecting;
    std::map<uint64_t, string> waiting_vc_ready;
    std::map<string, uint64_t> waiting_file_download;
    std::map<uint64_t, std::vector<string>> waiting_marker;
    std::vector<uint64_t> manually_paused;

    client.on_log(dpp::utility::cout_logger());

    client.on_ready([](const dpp::ready_t& event)
    {
        printf("SHARD: %d\nWS_PING: %f\n", event.shard_id, event.from->websocket_ping);
    });

    client.on_message_create([&mp_m, &manually_paused, &wm_m, &waiting_marker, &disconnecting, &connecting, &c_m, &dc_m, &dl_cv, &dl_m, &wd_m, &client, &sha_settings, &sha_id, &waiting_vc_ready, &waiting_file_download](const dpp::message_create_t& event)
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
            auto v = event.from->get_voice(event.msg.guild_id);
            if (v && !v->voiceclient->is_paused())
            {
                try
                {
                    auto u = mc::get_voice_from_gid(event.msg.guild_id, event.msg.author.id);
                    if (u.first->id != v->channel_id) return event.reply("You're not in my voice channel");
                }
                catch (const char* e)
                {
                    return event.reply("You're not in a voice channel");
                }
                v->voiceclient->pause_audio(true);
                bool p = true;
                {
                    std::lock_guard<std::mutex> lk(mp_m);
                    for (auto l = manually_paused.begin(); l != manually_paused.end(); l++)
                        if (*(l.base()) == event.msg.guild_id)
                        {
                            p = false;
                            break;
                        }
                    if (p) manually_paused.push_back(event.msg.guild_id);
                }
                event.reply("Paused");
            }
            else event.reply("I'm not playing anything");
        }
        else if (cmd == "resume")
        {
            auto v = event.from->get_voice(event.msg.guild_id);
            if (v)
            {
                if (v->voiceclient->is_paused())
                {
                    try
                    {
                        auto u = mc::get_voice_from_gid(event.msg.guild_id, event.msg.author.id);
                        if (u.first->id != v->channel_id) return event.reply("You're not in my voice channel");
                    }
                    catch (const char* e)
                    {
                        return event.reply("You're not in a voice channel");
                    }
                    v->voiceclient->pause_audio(false);
                    {
                        std::lock_guard<std::mutex> lk(mp_m);
                        for (auto l = manually_paused.begin(); l != manually_paused.end(); l++)
                            if (*(l.base()) == event.msg.guild_id)
                            {
                                manually_paused.erase(l);
                                break;
                            }
                    }
                    event.reply("Resumed");
                }
                else event.reply("I'm playing right now");
            }
            else event.reply("I'm not in any voice channel");
        }
        else if (cmd == "stop")
        {
            auto v = event.from->get_voice(event.msg.guild_id);
            if (v && (v->voiceclient->is_playing() || v->voiceclient->is_paused()))
            {
                try
                {
                    auto u = mc::get_voice_from_gid(event.msg.guild_id, event.msg.author.id);
                    if (u.first->id != v->channel_id) return event.reply("You're not in my voice channel");
                }
                catch (const char* e)
                {
                    return event.reply("You're not in a voice channel");
                }
                v->voiceclient->stop_audio();
                event.reply("Stopped");
            }
            else event.reply("I'm not playing anything right now");
        }
        else if (cmd == "skip")
        {
            auto v = event.from->get_voice(event.msg.guild_id);
            if (v)
            {
                try
                {
                    auto u = mc::get_voice_from_gid(event.msg.guild_id, event.msg.author.id);
                    if (u.first->id != v->channel_id) return event.reply("You're not in my voice channel");
                }
                catch (const char* e)
                {
                    return event.reply("You're not in a voice channel");
                }
                std::thread dc([](dpp::voiceconn* v) {
                    v->voiceclient->skip_to_next_marker();
                }, v);
                dc.detach();
                event.reply("Skipped");
            }
            else event.reply("I'm not in vc right now");
        }
        else if (cmd == "play")
        {
            dpp::guild* g = dpp::find_guild(event.msg.guild_id);
            std::pair<dpp::channel*, std::map<dpp::snowflake, dpp::voicestate>> vcuser;
            try
            {
                vcuser = mc::get_voice(g, event.msg.author.id);
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
                if (event.from->connecting_voice_channels.find(event.msg.guild_id) != event.from->connecting_voice_channels.end())
                {
                    printf("Disconnecting as not in vc but connected state still in cache: %ld\n", event.msg.guild_id);
                    event.from->disconnect_voice(event.msg.guild_id);
                }
            }

            // Client voice conn
            dpp::voiceconn* v = event.from->get_voice(event.msg.guild_id);
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
                if (v)
                {
                    printf("Disconnecting as no member in vc: %ld\n", event.msg.guild_id);
                    event.from->disconnect_voice(event.msg.guild_id);
                    disconnecting[event.msg.guild_id] = vcclient.first->id;
                }
            }

            if (v && v->voiceclient && v->voiceclient->get_tracks_remaining() > 0 && v->voiceclient->is_paused() && v->channel_id == vcuser.first->id)
            {
                {
                    std::lock_guard<std::mutex> lk(mp_m);
                    auto l = mc::vector_find(&manually_paused, event.msg.guild_id);
                    if (l != manually_paused.end())
                        manually_paused.erase(l);
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
                connecting[event.msg.guild_id] = vcuser.first->id;
                waiting_vc_ready[event.msg.guild_id] = fname;
            }
            // if (v)
            // {
            //     if (!v->is_ready())
            //     {
            //         printf("Set because not ready\n");
            //         waiting_vc_ready[event.msg.guild_id] = fname;
            //     }
            // }

            auto download = [&dl_cv, &waiting_file_download, &dl_m](string fname, string url, dpp::snowflake guild_id) {
                {
                    std::lock_guard<std::mutex> lk(dl_m);
                    waiting_file_download[fname] = guild_id;
                }
                string cmd = string("yt-dlp -f 251 -o - '") + url
                    + string("' | ffmpeg -i - -ar 48000 -ac 2 -sn -vn -c libopus -f ogg 'music/")
                    + std::regex_replace(
                        fname, std::regex("(')"), "'\\''",
                        std::regex_constants::match_any)
                    + string("'");
                printf("DOWNLOAD: \"%s\" \"%s\"\n", fname.c_str(), url.c_str());
                printf("CMD: %s\n", cmd.c_str());
                FILE* a = popen(cmd.c_str(), "w");
                pclose(a);
                {
                    std::lock_guard<std::mutex> lk(dl_m);
                    waiting_file_download.erase(fname);
                }
                dl_cv.notify_all();
            };

            auto start_stream = [&wm_m, &waiting_marker, &dl_cv, &waiting_file_download, &c_m, &connecting, &disconnecting, &dc_m, &waiting_vc_ready, &dl_m, &wd_m](const dpp::message_create_t event, string fname) {
                {
                    std::unique_lock lk(dc_m);
                    auto a = disconnecting.find(event.msg.guild_id);
                    if (a != disconnecting.end())
                    {
                        dl_cv.wait(lk, [&disconnecting, &event](){
                            return disconnecting.find(event.msg.guild_id) == disconnecting.end();
                        });
                    }
                }
                {
                    std::unique_lock lk(c_m);
                    auto a = connecting.find(event.msg.guild_id);
                    if (a != connecting.end())
                    {
                        event.from->connect_voice(event.msg.guild_id, a->second);
                        dl_cv.wait(lk, [&connecting, &event](){
                            return connecting.find(event.msg.guild_id) == connecting.end();
                        });
                    }
                }
                {
                    std::unique_lock<std::mutex> lk(wd_m);
                    auto a = waiting_vc_ready.find(event.msg.guild_id);
                    if (a != waiting_vc_ready.end())
                    {
                        printf("Waiting for ready state\n");
                        dl_cv.wait(lk, [&waiting_vc_ready, event]() {
                            auto c = waiting_vc_ready.find(event.msg.guild_id) == waiting_vc_ready.end();
                            printf("Checking for ready state: %d\n", c);
                            return c;
                        });
                    }
                }
                {
                    std::unique_lock<std::mutex> lk(dl_m);
                    auto a = waiting_file_download.find(fname);
                    if (a != waiting_file_download.end())
                    {
                        printf("Waiting for download\n");
                        dl_cv.wait(lk, [&waiting_file_download, event, fname]() {
                            auto c = waiting_file_download.find(fname) == waiting_file_download.end();
                            printf("Checking for download: %s \"%s\"\n", c ? "DONE" : "DOWNLOADING", fname.c_str());
                            return c;
                        });
                    }
                }
                printf("Attempt to stream\n");
                dpp::voiceconn* v = event.from->get_voice(event.msg.guild_id);
                // {
                //     std::unique_lock<std::mutex> lk(wm_m);
                //     if (waiting_marker.find(event.msg.guild_id) == waiting_marker.end())
                //     {
                //         std::vector<string> a;
                //         waiting_marker.insert({ event.msg.guild_id, a });
                //     }
                //     waiting_marker[event.msg.guild_id].push_back(fname);
                //     dl_cv.wait(lk, [&waiting_marker, fname, event]() {
                //         return true;
                //         auto l = waiting_marker.find(event.msg.guild_id);
                //         for (int i = 0; i < (int)(l->second.size()); i++)
                //             if (!l->second.at(i).empty())
                //                 if (l->second.at(i) == fname) return true;
                //                 else return false;
                //         return false;
                //     });
                // }

                // if (!v)
                try
                {
                    mc::stream(v, fname);
                }
                catch (int e)
                {
                    printf("ERROR_CODE: %d\n", e);
                    if (e == 2) event.reply("Can't access file");
                    else if (e == 1) event.reply("No connection");
                }
            };

            std::ifstream test((string("music/") + fname).c_str());
            if (!test.is_open())
            {
                event.reply("Downloading... Gimme 10 sec ight");
                auto url = result.url();
                std::thread t(download, fname, url, event.msg.guild_id);
                t.detach();
            }
            else test.close();

            std::thread st(start_stream, event, fname);
            st.detach();
        }
    });

    client.on_voice_ready([&wd_m, &dl_cv, &waiting_vc_ready](const dpp::voice_ready_t& event) {
        // dpp::voiceconn* v = event.from->get_voice(event.voice_client->server_id);
        {
            std::lock_guard<std::mutex> lk(wd_m);
            printf("on_voice_ready\n");
            auto w = waiting_vc_ready.find(event.voice_client->server_id);
            if (w != waiting_vc_ready.end())
            {
                waiting_vc_ready.erase(w);
                dl_cv.notify_all();
            }
        }
    });

    /* yt-dlp -f 251 'https://www.youtube.com/watch?v=FcRJGHkpm8s' -o - | ffmpeg -i - -ar 48000 -ac 2 -sn -dn -c [opus|libopus|copy] -f opus - */
    client.on_voice_state_update([&mp_m, &manually_paused, &connecting, &c_m, &disconnecting, &dc_m, &sha_id, &waiting_vc_ready, &dl_cv, &wd_m](const dpp::voice_state_update_t& event) {
        // printf("%ld JOIN/LEAVE %ld\n", event.state.user_id, event.state.channel_id);
        // Non client's user code
        if (event.state.user_id != sha_id)
        {
            dpp::voiceconn* v = event.from->get_voice(event.state.guild_id);
            // Pause audio when no user listening in vc
            if (v && v->channel_id && v->channel_id != event.state.channel_id && v->voiceclient && v->voiceclient->get_tracks_remaining() > 0 && !v->voiceclient->is_paused())
            {
                std::lock_guard<std::mutex> lk(mp_m);
                if (event.from && mc::vector_find(&manually_paused, event.state.guild_id) == manually_paused.end())
                {
                    try
                    {
                        auto voice = mc::get_voice_from_gid(event.state.guild_id, sha_id);
                        // Whether there's human listening in the vc
                        bool p = false;
                        for (auto l : voice.second)
                        {
                            // This only check user in cache, if user not in cache then skip
                            auto a = dpp::find_user(l.first);
                            if (a)
                            {
                                if (!a->is_bot())
                                {
                                    p = true;
                                    break;
                                }
                            }
                        }
                        if (!p)
                        {
                            v->voiceclient->pause_audio(true);
                            printf("Paused %ld as no user in vc\n", event.state.guild_id);
                        }
                    }
                    catch (const char* e)
                    {
                        printf("ERROR(main.405) %s\n", e);
                    }
                }
            }
            else
            {
                if (v && v->channel_id && v->channel_id == event.state.channel_id && v->voiceclient && v->voiceclient->get_tracks_remaining() > 0 && v->voiceclient->is_paused())
                {
                    std::lock_guard<std::mutex> lk(mp_m);
                    // Whether the track paused by user
                    auto l = mc::vector_find(&manually_paused, event.state.guild_id);
                    if (l == manually_paused.end()) v->voiceclient->pause_audio(false);
                }

            }

            // End non client's user code
            return;
        }

        // Client user code -----------------------------------

        if (!event.state.channel_id)
        {
            {
                std::lock_guard lk(dc_m);
                printf("on_voice_state_leave\n");
                auto a = disconnecting.find(event.state.guild_id);
                if (a != disconnecting.end())
                {
                    disconnecting.erase(event.state.guild_id);
                    dl_cv.notify_all();
                }
            }
            mc::reset_voice_channel(event.from, event.state.guild_id);
        }
        else
        {
            {
                std::lock_guard lk(c_m);
                printf("on_voice_state_join\n");
                auto a = connecting.find(event.state.guild_id);
                if (a != connecting.end())
                {
                    connecting.erase(event.state.guild_id);
                    dl_cv.notify_all();
                }
            }
        }
    });

    // client.on_voice_track_marker([&wm_m, &dl_cv, &waiting_marker](const dpp::voice_track_marker_t& event) {
    //     {
    //         std::lock_guard<std::mutex> lk(wm_m);
    //         printf("on_voice_track_marker\n");
    //         std::vector<std::string> l = waiting_marker.find(event.voice_client->server_id)->second;
    //         for (int i = 0; i < (int)(l.size()); i++)
    //         {
    //             printf("%s | %s\n", l.at(i).c_str(), event.track_meta.c_str());
    //             if (l.at(i) == event.track_meta)
    //                 l.erase(l.begin() + i);
    //         }
    //         dl_cv.notify_all();
    //     }
    // });

    client.start(false);
    return 0;
}
