#include <stdio.h>
#include <string>
#include <vector>
#include <regex>
#include <dpp/dpp.h>
#include "nlohmann/json.hpp"
#include "musicat/musicat.h"
#include "musicat/player.h"
#include "musicat/cmds.h"

namespace mc = musicat;
namespace mpl = musicat_player;
namespace mcmd = musicat_command;

using json = nlohmann::json;
using string = std::string;

bool running = true;

void on_sigint(int code) {
    printf("RECEIVED SIGINT\nCODE: %d\n", code);
    running = false;
}

int main(int argc, const char* argv[])
{
    signal(SIGINT, on_sigint);

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
        int ret = mc::cli(client, sha_id, argc, argv, &running);
        while (running) sleep(1);
        return ret;
    }

    printf("GLOBAL_PREFIX: %s\n", sha_settings.defaultPrefix.c_str());

    std::shared_ptr<mpl::Manager> player_manager = std::make_shared<mpl::Manager>(&client, sha_id);

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

    client.on_autocomplete([&player_manager, &client, &sha_settings, &sha_id](const dpp::autocomplete_t& event) {
        const string cmd = event.name;
        string opt = "";
        string param = "";

        for (const auto& i : event.options)
        {
            if (i.focused)
            {
                opt = i.name;
                if (i.value.index()) param = std::get<string>(i.value);
                break;
            }
        }

        if (opt.length())
        {
            if (cmd == "play" && opt == "query") mcmd::play::autocomplete::query(event, param, player_manager, client);
        }
    });

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

        if (cmd == "hello") mcmd::hello::slash_run(event);
        else if (cmd == "why") event.reply("Why not");
        else if (cmd == "hi") event.reply("HIII");
        else if (cmd == "invite") mcmd::invite::slash_run(event);
        else if (cmd == "support") event.reply("https://www.discord.gg/vpk2KyKHtu");
        else if (cmd == "repo") event.reply("https://github.com/Neko-Life/Musicat");
        else if (cmd == "pause") mcmd::pause::slash_run(event, player_manager);
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
        else if (cmd == "skip") mcmd::skip::slash_run(event, player_manager);
        else if (cmd == "play") mcmd::play::slash_run(event, player_manager, sha_id);
        else if (cmd == "loop") mcmd::loop::slash_run(event, player_manager, sha_id);
        else if (cmd == "queue") mcmd::queue::slash_run(event, player_manager);
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

    while (running) sleep(1);

    return 0;
}
