#include <stdio.h>
#include <string>
#include <vector>
#include <regex>
#include <dpp/dpp.h>
#include "nlohmann/json.hpp"
#include "musicat/musicat.h"
#include "musicat/player.h"
#include "musicat/slash.h"

#define PRINT_USAGE_REGISTER_SLASH printf("Usage:\n  reg <guild_id|\"g\">\n")

namespace mc = musicat;
using json = nlohmann::json;
using string = std::string;

bool running = true;

void on_sigint(int code) {
    printf("RECEIVED SIGINT\nCODE: %d\n", code);
    running = false;
}

int main(int argc, const char* argv[])
{
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

    if (argc > 1)
    {
        string a1 = string(argv[1]);
        // for (int i = 1; i <= argc; i++){
        //     auto a=argv[i];
        //     if (a)
        // }
        int cmd = -1;
        if (a1 == "reg") cmd = 0;

        if (cmd < 0)
        {
            PRINT_USAGE_REGISTER_SLASH;
            return 0;
        }

        switch (cmd)
        {
        case 0:
            if (argc == 2)
            {
                printf("Provide guild_id or \"g\" to register globally\n");
                return 0;
            }
            string a2 = string(argv[2]);
            if (a2 == "g")
            {
                printf("Registering commands globally...\n");
                auto c = sha_slash::get_all(sha_id);
                client.global_bulk_command_create(c);
                return 0;
            }
            else
            {
                if (!std::regex_match(argv[2], std::regex("^\\d{17,20}$"), std::regex_constants::match_any))
                {
                    printf("Provide valid guild_id\n");
                    return 0;
                }
                int64_t gid;
                std::istringstream iss(argv[2]);
                iss >> gid;
                if (gid < 0)
                {
                    printf("Invalid integer, too large\n");
                    return 0;
                }
                printf("Registering commands in %ld\n", gid);
                auto c = sha_slash::get_all(sha_id);
                client.guild_bulk_command_create(c, gid);
                return 0;
            }
            break;
        }

        return 0;
    }

    printf("GLOBAL_PREFIX: %s\n", sha_settings.defaultPrefix.c_str());

    Sha_Player_Manager* player_manager = new Sha_Player_Manager(&client, sha_id);

    client.on_log(dpp::utility::cout_logger());

    client.on_ready([](const dpp::ready_t& event)
    {
        printf("SHARD: %d\nWS_PING: %f\n", event.shard_id, event.from->websocket_ping);
    });

    client.on_message_create([](const dpp::message_create_t& event) {
        // Update channel last message Id
        auto c = dpp::find_channel(event.msg.channel_id);
        if (c) c->last_message_id = event.msg.id;
    });

    // client.on_autocomplete

    client.on_interaction_create([&player_manager, &client, &sha_settings, &sha_id](const dpp::interaction_create_t& event)
    {
        if (!event.command.guild_id) return;
        if (event.command.usr.is_bot()) return;

        auto cmd = event.command.get_command_name();

        // std::smatch content_regex_results;
        // {
        //     string cstr = string("^((?:")
        //         .append(sha_settings.get_prefix(event.command.guild_id))
        //         .append("|<@\\!?")
        //         .append(std::to_string(sha_id))
        //         .append(">)\\s*)([^\\s]*)(\\s+)?(.*)");
        //     std::regex re(cstr.c_str(), std::regex_constants::ECMAScript | std::regex_constants::icase);
        //     auto reFlag = std::regex_constants::match_not_null | std::regex_constants::match_any;
        //     std::regex_search(event.command.content, content_regex_results, re, reFlag);
        // }

        // if (!content_regex_results.size()) return;

        // string prefix = content_regex_results[1].str();
        // string cmd = content_regex_results[2].str();
        // for (size_t s = 0; s < cmd.length(); s++)
        // {
        //     cmd[s] = std::tolower(cmd[s]);
        // }
        // string cmd_args = content_regex_results[4].str();

        if (cmd == "hello") event.reply("Hello there!!");
        else if (cmd == "why") event.reply("Why not");
        else if (cmd == "hi") event.reply("HIII");
        else if (cmd == "invite") event.reply("https://discord.com/api/oauth2/authorize?client_id=960168583969767424&permissions=412353875008&scope=bot%20applications.commands");
        else if (cmd == "support") event.reply("https://www.discord.gg/vpk2KyKHtu");
        else if (cmd == "repo") event.reply("https://github.com/Neko-Life/Musicat");
        else if (cmd == "pause")
        {
            if (!player_manager->voice_ready(event.command.guild_id))
            {
                event.reply("Please wait while I'm getting ready to stream");
                return;
            }
            try
            {
                if (player_manager->pause(event.from, event.command.guild_id, event.command.usr.id)) event.reply("Paused");
                else event.reply("I'm not playing anything");
            }
            catch (mc::exception& e)
            {
                return event.reply(e.what());
            }
        }
        // else if (cmd == "resume")
        // {
        //     auto v = event.from->get_voice(event.command.guild_id);
        //     if (v)
        //     {
        //         if (v->voiceclient->is_paused())
        //         {
        //             try
        //             {
        //                 auto u = mc::get_voice_from_gid(event.command.guild_id, event.command.usr.id);
        //                 if (u.first->id != v->channel_id) return event.reply("You're not in my voice channel");
        //             }
        //             catch (const char* e)
        //             {
        //                 return event.reply("You're not in a voice channel");
        //             }
        //             v->voiceclient->pause_audio(false);
        //             {
        //                 std::lock_guard<std::mutex> lk(player_manager->mp_m);
        //                 auto l = mc::vector_find(&manually_paused, event.command.guild_id);
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
        //     auto v = event.from->get_voice(event.command.guild_id);
        //     if (v && (v->voiceclient->is_playing() || v->voiceclient->is_paused()))
        //     {
        //         try
        //         {
        //             auto u = mc::get_voice_from_gid(event.command.guild_id, event.command.usr.id);
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
            if (!player_manager->voice_ready(event.command.guild_id))
            {
                event.reply("Please wait while I'm getting ready to stream");
                return;
            }
            try
            {
                auto v = event.from->get_voice(event.command.guild_id);
                if (player_manager->skip(v, event.command.guild_id, event.command.usr.id))
                {
                    event.reply("Skipped");
                }
                else event.reply("I'm not playing anything");
            }
            catch (mc::exception& e)
            {
                return event.reply(e.what());
            }
        }
        else if (cmd == "play")
        {
            if (!player_manager->voice_ready(event.command.guild_id))
            {
                event.reply("Please wait while I'm getting ready to stream");
                return;
            }

            auto guild_id = event.command.guild_id;
            auto from = event.from;
            auto user_id = event.command.usr.id;
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
                std::lock_guard lk(player_manager->dc_m);
                from->disconnect_voice(guild_id);
                player_manager->disconnecting[guild_id] = vcclient.first->id;
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
                if (v && arg_query.length() > 0)
                {
                    printf("Disconnecting as no member in vc: %ld\n", guild_id);
                    if (v && v->voiceclient && v->voiceclient->get_secs_remaining() > 0.1)
                        player_manager->stop_stream(guild_id);

                    std::lock_guard lk(player_manager->dc_m);
                    // FIXME: It WILL segvault (with gdb?) if you wait a song until it ends and move to other vc and play another song that trigger this
                    from->disconnect_voice(guild_id);
                    player_manager->disconnecting[guild_id] = vcclient.first->id;
                }
            }

            if (v && v->voiceclient && v->voiceclient->is_paused() && v->channel_id == vcuser.first->id)
            {
                player_manager->unpause(v->voiceclient, event.command.guild_id);
                if (arg_query.length() == 0) return event.reply("Resumed");
            }

            auto op = player_manager->get_player(event.command.guild_id);

            if (op && v && v->voiceclient && op->queue->size() && !v->voiceclient->is_paused() && !v->voiceclient->is_playing()) v->voiceclient->insert_marker("c");

            if (arg_query.length() == 0) return event.reply("Provide song query if you wanna add a song, may be URL or song name");

            event.thinking();
            auto searches = yt_search(arg_query).trackResults();
            if (!searches.size()) return event.edit_response("Can't find anything");
            auto result = searches.front();
            string fname = std::regex_replace(result.title() + string("-") + result.id() + string(".ogg"), std::regex("/"), "", std::regex_constants::match_any);

            if (vcclient_cont == false || !v)
            {
                std::lock_guard<std::mutex> lk2(player_manager->c_m);
                std::lock_guard<std::mutex> lk3(player_manager->wd_m);
                player_manager->connecting[guild_id] = vcuser.first->id;
                printf("INSERTING WAIT FOR VC READY\n");
                player_manager->waiting_vc_ready[guild_id] = fname;
            }

            bool dling = false;

            std::ifstream test((string("music/") + fname).c_str());
            if (!test.is_open())
            {
                dling = true;
                event.edit_response(string("Downloading ") + result.title() + string("... Gimme 10 sec ight"));
                if (player_manager->waiting_file_download.find(fname) == player_manager->waiting_file_download.end())
                {
                    auto url = result.url();
                    player_manager->download(fname, url, guild_id);
                }
            }
            else
            {
                test.close();
                event.edit_response(string("Added: ") + result.title());
            }

            std::thread pjt([&player_manager, from, guild_id]() {
                player_manager->reconnect(from, guild_id);
            });
            pjt.detach();

            std::thread dlt([&player_manager, sha_id](dpp::discord_client* from, bool dling, YTrack result, string fname, dpp::snowflake user_id, dpp::snowflake guild_id, dpp::snowflake channel_id, bool arg_top) {
                if (dling) player_manager->wait_for_download(fname);
                auto p = player_manager->create_player(guild_id);

                Sha_Track t(result);
                t.filename = fname;
                t.user_id = user_id;
                p->add_track(t, arg_top);
                p->set_channel(channel_id);

                std::pair<dpp::channel*, std::map<dpp::snowflake, dpp::voicestate>> vu;
                bool b = true;
                try { vu = mc::get_voice_from_gid(guild_id, sha_id); }
                catch (const char* e) { b = false; }

                dpp::voiceconn* v = from->get_voice(guild_id);
                if (b && mc::has_listener(&vu.second)
                    && v && v->voiceclient && v->voiceclient->get_secs_remaining() < 0.1)
                    v->voiceclient->insert_marker("s");

            }, from, dling, result, fname, event.command.usr.id, event.command.guild_id, event.command.channel_id, arg_top);
            dlt.detach();
        }
        else if (cmd == "loop")
        {
            if (!player_manager->voice_ready(event.command.guild_id))
            {
                event.reply("Please wait while I'm getting ready to stream");
                return;
            }
            static const char* loop_message[] = { "Turned off repeat mode", "Set to repeat a song", "Set to repeat queue", "Set to repeat a song and not to remove skipped song" };
            std::pair<dpp::channel*, std::map<dpp::snowflake, dpp::voicestate>> uvc;
            std::pair<dpp::channel*, std::map<dpp::snowflake, dpp::voicestate>> cvc;
            try
            {
                uvc = mc::get_voice_from_gid(event.command.guild_id, event.command.usr.id);
            }
            catch (const char* e)
            {
                return event.reply("You're not in a voice channel");
            }
            try
            {
                cvc = mc::get_voice_from_gid(event.command.guild_id, sha_id);
            }
            catch (const char* e)
            {
                return event.reply("I'm not playing anything right now");
            }
            if (uvc.first->id != cvc.first->id) return event.reply("You're not in my voice channel");
            int64_t a_l = 0;
            mc::get_inter_param(event, "mode", &a_l);

            auto player = player_manager->create_player(event.command.guild_id);
            if (player->loop_mode == a_l)
            {
                event.reply("Already set to that mode");
                return;
            }

            player->set_loop_mode(a_l);
            event.reply(loop_message[a_l]);
            try
            {
                player_manager->update_info_embed(event.command.guild_id);
            }
            catch (...)
            {
                // Meh
            }
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
        printf("To handle voice track marker\n");
        player_manager->handle_on_track_marker(event);
    });

    client.on_message_delete([&player_manager](const dpp::message_delete_t& event) {
        player_manager->handle_on_message_delete(event);
    });

    client.on_message_delete_bulk([&player_manager](const dpp::message_delete_bulk_t& event) {
        player_manager->handle_on_message_delete_bulk(event);
    });

    // client.set_websocket_protocol(dpp::websocket_protocol_t::ws_etf);

    client.start(true);

    signal(SIGINT, on_sigint);

    while (running) sleep(1);

    delete player_manager;

    return 0;
}
