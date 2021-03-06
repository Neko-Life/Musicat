#include <stdio.h>
#include <string>
#include <vector>
#include <regex>
#include <chrono>
#include <dpp/dpp.h>
#include <any>
#include "nlohmann/json.hpp"
#include "musicat/musicat.h"
#include "musicat/player.h"
#include "musicat/cmds.h"
#include "musicat/pagination.h"
#include "musicat/storage.h"

#define ONE_HOUR_SECOND 3600

namespace musicat {
    namespace mpl = musicat_player;
    namespace mcmd = musicat_command;

    using json = nlohmann::json;
    using string = std::string;

    bool running = true;

    void on_sigint(int code) {
        printf("RECEIVED SIGINT\nCODE: %d\n", code);
        running = false;
    }

    int run(int argc, const char* argv[])
    {
        signal(SIGINT, on_sigint);

        json sha_cfg;
        {
            std::ifstream scs("sha_conf.json");
            scs >> sha_cfg;
            scs.close();
        }

        dpp::cluster client(sha_cfg["SHA_TKN"], dpp::i_message_content | dpp::i_guild_members | dpp::i_default_intents);

        settings sha_settings;
        dpp::snowflake sha_id(sha_cfg["SHA_ID"]);
        sha_settings.defaultPrefix = sha_cfg["SHA_PREFIX"];

        if (argc > 1)
        {
            int ret = cli(client, sha_id, argc, argv, &running);
            while (running) sleep(1);
            return ret;
        }

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

        client.on_button_click([](const dpp::button_click_t& event) {
            const size_t fsub = event.custom_id.find("/");
            const string cmd = event.custom_id.substr(0, fsub);

            if (!cmd.length()) return;

            if (cmd == "page_queue")
            {
                const string param = event.custom_id.substr(fsub + 1, 1);
                if (!param.length())
                {
                    fprintf(stderr, "[WARN(run.78)] command \"page_queue\" have no param\n");
                    return;
                }
                event.reply(dpp::ir_deferred_update_message, "");
                // dpp::message* m = new dpp::message(event.command.msg);
                paginate::update_page(event.command.msg.id, param);//, m);
            }
            else if (cmd == "modal_p")
            {
                const string param = event.custom_id.substr(fsub + 1, string::npos);
                if (!param.length())
                {
                    fprintf(stderr, "[WARN(run.87)] command \"modal_p\" have no param\n");
                    return;
                }
                if (param == "que_s_track")
                {
                    event.dialog(mcmd::search::modal_enqueue_searched_track());
                }
                else
                {
                    fprintf(stderr, "[WARN(run.98)] modal_p param isn't handled: \"%s\"\n", param.c_str());
                }
            }
        });

        client.on_form_submit([&player_manager](const dpp::form_submit_t& event) {
            printf("[FORM] %s %ld\n", event.custom_id.c_str(), event.command.message_id);
            if (event.custom_id == "modal_p")
            {
                if (event.components.size())
                {
                    auto comp = event.components.at(0).components.at(0);
                    if (comp.custom_id == "que_s_track")
                    {
                        if (!comp.value.index()) return;
                        string q = std::get<string>(comp.value);
                        if (!q.length()) return;

                        int64_t pos = 0;
                        sscanf(q.c_str(), "%ld", &pos);
                        if (pos < 1) return;

                        auto storage = storage::get(event.command.message_id);
                        if (!storage.has_value())
                        {
                            event.reply("It seems like this result is outdated, try make a new search");
                            return;
                        }

                        auto tracks = std::any_cast<std::vector<yt_search::YTrack>>(storage);
                        if (tracks.size() < (size_t)pos) return;
                        auto result = tracks.at(pos - 1);

                        auto from = event.from;
                        auto guild_id = event.command.guild_id;
                        const string prepend_name = string("<@") + std::to_string(event.command.usr.id) + string("> ");

                        event.thinking();
                        string fname = std::regex_replace(result.title() + string("-") + result.id() + string(".ogg"), std::regex("/"), "", std::regex_constants::match_any);
                        bool dling = false;

                        std::ifstream test((string("music/") + fname).c_str());
                        if (!test.is_open())
                        {
                            dling = true;
                            event.edit_response(prepend_name + string("Downloading ") + result.title() + string("... Gimme 10 sec ight"));
                            if (player_manager->waiting_file_download.find(fname) == player_manager->waiting_file_download.end())
                            {
                                auto url = result.url();
                                player_manager->download(fname, url, guild_id);
                            }
                        }
                        else
                        {
                            test.close();
                            event.edit_response(prepend_name + string("Added: ") + result.title());
                        }

                        std::thread dlt([prepend_name, player_manager, dling, fname, guild_id, from](const dpp::interaction_create_t event, yt_search::YTrack result) {
                            dpp::snowflake user_id = event.command.usr.id;
                            auto p = player_manager->create_player(guild_id);
                            if (dling)
                            {
                                player_manager->wait_for_download(fname);
                                event.edit_response(prepend_name + string("Added: ") + result.title());
                            }
                            if (from) p->from = from;

                            mpl::MCTrack t(result);
                            t.filename = fname;
                            t.user_id = user_id;
                            p->add_track(t, false, guild_id, dling);
                        }, event, result);
                        dlt.detach();
                    }
                }
                else
                {
                    fprintf(stderr, "[WARN(run.112)] Form modal_p doesn't contain any components row\n");
                }
            }
        });

        client.on_autocomplete([&player_manager, &client, &sha_settings](const dpp::autocomplete_t& event) {
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
                if (cmd == "play")
                {
                    if (opt == "query") mcmd::play::autocomplete::query(event, param, player_manager, client);
                }
                // else if (cmd == "move")
                // {
                //     mcmd::move::autocomplete::track(event, param, player_manager, client);
                // }
            }
        });

        client.on_interaction_create([&player_manager, &client, &sha_settings](const dpp::interaction_create_t& event)
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
            //                 auto u = get_voice_from_gid(event.command.guild_id, event.command.usr.id);
            //                 if (u.first->id != v->channel_id) return event.reply("You're not in my voice channel");
            //             }
            //             catch (const char* e)
            //             {
            //                 return event.reply("You're not in a voice channel");
            //             }
            //             v->voiceclient->pause_audio(false);
            //             {
            //                 std::lock_guard<std::mutex> lk(player_manager->mp_m);
            //                 auto l = vector_find(&manually_paused, event.command.guild_id);
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
            //             auto u = get_voice_from_gid(event.command.guild_id, event.command.usr.id);
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
            else if (cmd == "play") mcmd::play::slash_run(event, player_manager);
            else if (cmd == "loop") mcmd::loop::slash_run(event, player_manager);
            else if (cmd == "queue") mcmd::queue::slash_run(event, player_manager);
            else if (cmd == "autoplay") mcmd::autoplay::slash_run(event, player_manager);
            else if (cmd == "move") mcmd::move::slash_run(event, player_manager);
            else if (cmd == "remove") mcmd::remove::slash_run(event, player_manager);
            else if (cmd == "bubble_wrap") mcmd::bubble_wrap::slash_run(event);
            else if (cmd == "search") mcmd::search::slash_run(event);
            else
            {
                event.reply("Seems like somethin's wrong here, I can't find that command anywhere in my database");
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
            player_manager->handle_on_track_marker(event, player_manager);
        });

        client.on_message_delete([&player_manager](const dpp::message_delete_t& event) {
            player_manager->handle_on_message_delete(event);
            paginate::handle_on_message_delete(event);
        });

        client.on_message_delete_bulk([&player_manager](const dpp::message_delete_bulk_t& event) {
            player_manager->handle_on_message_delete_bulk(event);
            paginate::handle_on_message_delete_bulk(event);
        });

        // client.set_websocket_protocol(dpp::websocket_protocol_t::ws_etf);

        client.start(true);

        time_t last_gc;
        time(&last_gc);

        while (running)
        {
            sleep(5);

            // GC
            if ((time(NULL) - last_gc) > ONE_HOUR_SECOND)
            {
                printf("[GC] Starting scheduled gc\n");
                auto start_time = std::chrono::high_resolution_clock::now();
                // gc codes
                paginate::gc();

                // reset last_gc
                time(&last_gc);
                auto end_time = std::chrono::high_resolution_clock::now();
                auto done = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
                printf("[GC] Ran for %ld ms\n", done.count());
            }
        }

        return 0;
    }
}
