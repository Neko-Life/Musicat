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

    std::mutex dl_m;
    std::mutex wd_m;
    std::mutex c_m;
    std::mutex dc_m;
    std::condition_variable dl_cv;

    std::map<uint64_t, uint64_t> connecting;
    std::map<uint64_t, uint64_t> disconnecting;
    std::map<uint64_t, string> waiting_vc_ready;
    std::map<uint64_t, string> waiting_file_download;

    client.on_log(dpp::utility::cout_logger());

    client.on_ready([](const dpp::ready_t& event)
    {
        printf("SHARD: %d\nWS_PING: %f\n", event.shard_id, event.from->websocket_ping);
    });

    client.on_message_create([&disconnecting, &connecting, &c_m, &dc_m, &dl_cv, &dl_m, &wd_m, &client, &sha_settings, &sha_id, &waiting_vc_ready, &waiting_file_download](const dpp::message_create_t& event)
    {
        if (event.msg.author.is_bot()) return;

        string cstr = string("^((?:")
            .append(sha_settings.get_prefix(event.msg.guild_id))
            .append("|<@\\!?")
            .append(std::to_string(sha_id))
            .append(">)\\s*)([^\\s]*)(\\s+)?(.*)");

        std::regex re(cstr.c_str(), std::regex_constants::ECMAScript | std::regex_constants::icase);

        auto reFlag = std::regex_constants::match_not_null | std::regex_constants::match_any;
        std::smatch cm;
        std::regex_search(event.msg.content, cm, re, reFlag);

        if (!cm.size()) return;

        string prefix = cm[1].str();
        string cmdName = cm[2].str();
        for (size_t s = 0; s < cmdName.length(); s++)
        {
            cmdName[s] = std::tolower(cmdName[s]);
        }
        string fullArg = cm[4].str();

        if (cmdName == "hello") event.reply("Hello there!!");
        else if (cmdName == "why") event.reply("Why not");
        else if (cmdName == "hi") event.reply("HIII");
        else if (cmdName == "invite") event.reply("https://discord.com/api/oauth2/authorize?client_id=960168583969767424&permissions=412353875008&scope=bot%20applications.commands");
        else if (cmdName == "support") event.reply("https://www.discord.gg/vpk2KyKHtu");
        else if (cmdName == "repo") event.reply("https://github.com/Neko-Life/Musicat");
        else if (cmdName == "play")
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
            bool vcclient_cont = true;
            try { vcclient = mc::get_voice(g, sha_id); }
            catch (const char* e)
            {
                vcclient_cont = false;
            }
            dpp::voiceconn* v = event.from->get_voice(event.msg.guild_id);
            if (vcclient_cont && vcclient.first->id != vcuser.first->id)
            {
                if (vcclient.second.size() > 1)
                {
                    for (auto& r : vcclient.second)
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
                                return;
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
                    event.from->disconnect_voice(event.msg.guild_id);
                    disconnecting[event.msg.guild_id] = vcclient.first->id;
                }
            }

            if (fullArg.length() == 0) return event.reply("Provide song query if you wanna add a song, may be URL or song name");

            event.reply("Searching...");

            auto result = yt_search(fullArg).trackResults().front();
            string fname = std::regex_replace(result.title() + string("-") + result.id() + string(".ogg"), std::regex("/"), "", std::regex_constants::match_any);
            event.reply(string("Added: ") + result.title());

            if (vcclient_cont == false || !v)
            {
                connecting[event.msg.guild_id] = vcuser.first->id;
                waiting_vc_ready[event.msg.guild_id] = fname;
            }
            if (v)
            {
                if (!v->is_ready())
                {
                    printf("Set because not ready\n");
                    waiting_vc_ready[event.msg.guild_id] = fname;
                }
            }

            auto download = [&dl_cv, &waiting_file_download, &dl_m](string fname, string url, dpp::snowflake guild_id) {
                std::lock_guard<std::mutex> lk(dl_m);
                waiting_file_download[guild_id] = fname;
                string cmd = string("yt-dlp -f 251 -o - \"") + url + string("\" | ffmpeg -i - -ar 48000 -b:a 128000 -ac 2 -sn -dn -c libopus -f ogg \"music/") + std::regex_replace(fname, std::regex("(\")"), "\\\"", std::regex_constants::match_any) + string("\"");
                printf("DOWNLOAD: \"%s\" \"%s\"\n", fname.c_str(), url.c_str());
                printf("CMD: %s\n", cmd.c_str());
                FILE* a = popen(cmd.c_str(), "w");
                pclose(a);
                waiting_file_download.erase(guild_id);
                dl_cv.notify_one();
            };

            auto start_stream = [&dl_cv, &waiting_file_download, &c_m, &connecting, &disconnecting, &dc_m, &waiting_vc_ready, &dl_m, &wd_m](const dpp::message_create_t event, string fname) {
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
                if (waiting_vc_ready.find(event.msg.guild_id) != waiting_vc_ready.end())
                {
                    std::unique_lock<std::mutex> lk(wd_m);
                    printf("Waiting for ready state\n");
                    dl_cv.wait(lk, [&waiting_vc_ready, event]() {
                        auto c = waiting_vc_ready.find(event.msg.guild_id) == waiting_vc_ready.end();
                        printf("Checking for ready state: %d\n", c);
                        return c;
                    });
                }
                if (waiting_file_download.find(event.msg.guild_id) != waiting_file_download.end())
                {
                    std::unique_lock<std::mutex> lk(dl_m);
                    printf("Waiting for download\n");
                    dl_cv.wait(lk, [&waiting_file_download, event]() {
                        auto c = waiting_file_download.find(event.msg.guild_id) == waiting_file_download.end();
                        printf("Checking for download: %d\n", c);
                        return c;
                    });
                }
                printf("Attempt to stream\n");
                dpp::voiceconn* v = event.from->get_voice(event.msg.guild_id);

                // if (!v)
                try
                {
                    mc::stream(v, fname);
                }
                catch (int e)
                {
                    printf("ERROR_CODE: %d\n", e);
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
                dl_cv.notify_one();
            }
        }
    });

    /* yt-dlp -f 251 'https://www.youtube.com/watch?v=FcRJGHkpm8s' -o - | ffmpeg -i - -ar 48000 -ac 2 -sn -dn -c [opus|libopus|copy] -f opus - */
    client.on_voice_state_update([&connecting, &c_m, &disconnecting, &dc_m, &sha_id, &waiting_vc_ready, &dl_cv, &wd_m](const dpp::voice_state_update_t& event) {
        // printf("%ld JOIN/LEAVE %ld\n", event.state.user_id, event.state.channel_id);
        if (event.state.user_id != sha_id) return;
        if (!event.state.channel_id)
        {
            {
                std::lock_guard lk(dc_m);
                printf("on_voice_state_leave\n");
                auto a = disconnecting.find(event.state.guild_id);
                if (a != disconnecting.end())
                {
                    disconnecting.erase(event.state.guild_id);
                    dl_cv.notify_one();
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
                    dl_cv.notify_one();
                }
            }
        }
    });

    client.start(false);
    return 0;
}
