#include <stdio.h>
#include <chrono>
#include <dpp/dpp.h>
#include <fstream>
#include <string>
#include <vector>
#include <stdexcept>
#include <regex>
#include <cstdio>
#include <ogg/ogg.h>
#include <opus/opusfile.h>
#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include "nlohmann/json.hpp"
#include "encode.h"

#ifndef MUSICAT_H
#define MUSICAT_H

namespace mc
{

    using string = std::string;

    template <typename T>
    auto vector_has(std::vector<T>* _vec, T _find) {
        auto i = _vec->begin();
        for (; i != _vec->end();i++)
        {
            if (*i == _find) return i;
        }
        return i;
    }

    struct settings
    {
        string defaultPrefix;

        std::map<dpp::snowflake, string> prefix;
        std::vector<dpp::snowflake> joining_list;

        string get_prefix(const dpp::snowflake guildId)
        {
            auto gid = prefix.find(guildId);
            if (gid == prefix.end())
                return defaultPrefix;
            return gid->second;
        }

        auto set_prefix(const dpp::snowflake guildId, const string newPrefix)
        {
            return prefix.insert_or_assign(guildId, newPrefix);
        }

        void set_joining_vc(dpp::snowflake vc_id) {
            if (mc::vector_has(&joining_list, vc_id) != joining_list.end()) return;
            joining_list.emplace_back(vc_id);
        }

        void remove_joining_vc(dpp::snowflake vc_id) {
            auto i = mc::vector_has(&joining_list, vc_id);
            if (i != joining_list.end())
            {
                joining_list.erase(i);
                printf("ERASE IS CALLED\n");
            }
        }
    };

    /**
     * @brief Destroy and reset connecting state of the guild, must be invoked when failed to join or left a vc
     *
     * @param client The client
     * @param guild_id Guild Id of the vc client connecting in
     */
    void reset_voice_channel(dpp::discord_client* client, dpp::snowflake guild_id) {
        auto v = client->connecting_voice_channels.find(guild_id);
        if (v == client->connecting_voice_channels.end()) return;
        delete v->second;
        v->second = nullptr;
        client->connecting_voice_channels.erase(v);
    }

    /**
     * @brief Get the voice object and connected voice members a vc of a guild
     *
     * @param guild Guild the member in
     * @param user_id Target member
     * @return std::pair<dpp::channel*, std::map<dpp::snowflake, dpp::voicestate>>
     * @throw const char* User isn't in vc
     */
    std::pair<dpp::channel*, std::map<dpp::snowflake, dpp::voicestate>> get_voice(dpp::guild* guild, dpp::snowflake user_id) {
        for (auto& fc : guild->channels)
        {
            auto gc = dpp::find_channel(fc);
            if (!gc || (!gc->is_voice_channel() && !gc->is_stage_channel())) continue;
            std::map<dpp::snowflake, dpp::voicestate> vm = gc->get_voice_members();
            if (vm.find(user_id) != vm.end())
            {
                return { gc,vm };
            }
        }
        throw "User not in vc";
    }

    /**
     * @brief Get the voice object and connected voice members a vc of a guild
     *
     * @param guild_id Guild Id the member in
     * @param user_id Target member
     * @return std::pair<dpp::channel*, std::map<dpp::snowflake, dpp::voicestate>>
     * @throw const char* Unknown guild or user isn't in vc
     */
    std::pair<dpp::channel*, std::map<dpp::snowflake, dpp::voicestate>> get_voice_from_gid(dpp::snowflake guild_id, dpp::snowflake user_id) {
        dpp::guild* g = dpp::find_guild(guild_id);
        if (g) return get_voice(g, user_id);
        throw "Unknown guild";
    }

    /**
     * @brief Execute shell cmd and return anything it printed to console
     *
     * @param cmd Command
     * @return string
     * @throw const char* Exec failed (can't call popen or unknown command)
     */
    string exec(string cmd) {
        if (!cmd.length()) throw "LEN";
        std::array<char, 128> buffer;
        string res;
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
        if (!pipe) throw "ERROR: exec failed";
        while (fgets(buffer.data(), buffer.size(), pipe.get())) res += buffer.data();
        return res;
    }

    void stream(dpp::voiceconn* v, string fname) {
        if (v && v->voiceclient && v->voiceclient->is_ready())
        {
            auto start_time = std::chrono::high_resolution_clock::now();
            printf("Streaming \"%s\" to %ld\n", fname.c_str(), v->voiceclient->server_id);
            {
                std::ifstream fs((string("music/") + fname).c_str());
                if (!fs.is_open()) throw 2;
                else fs.close();
            }
            FILE* fd = fopen((string("music/") + fname).c_str(), "rb");

            printf("Initializing buffer\n");
            ogg_sync_state oy;
            ogg_stream_state os;
            ogg_page og;
            ogg_packet op;
            // OpusHead header;
            char* buffer;

            fseek(fd, 0L, SEEK_END);
            size_t sz = ftell(fd) + BUFSIZ;
            printf("SIZE_T: %ld\n", sz);
            rewind(fd);

            ogg_sync_init(&oy);

            // int eos = 0;
            // int i;

            buffer = ogg_sync_buffer(&oy, sz);
            fread(buffer, 1, sz, fd);

            ogg_sync_wrote(&oy, sz);

            if (ogg_sync_pageout(&oy, &og) != 1)
            {
                fprintf(stderr, "Does not appear to be ogg stream.\n");
                exit(1);
            }

            ogg_stream_init(&os, ogg_page_serialno(&og));

            if (ogg_stream_pagein(&os, &og) < 0)
            {
                fprintf(stderr, "Error reading initial page of ogg stream.\n");
                exit(1);
            }

            if (ogg_stream_packetout(&os, &op) != 1)
            {
                fprintf(stderr, "Error reading header packet of ogg stream.\n");
                exit(1);
            }

            /* We must ensure that the ogg stream actually contains opus data */
            // if (!(op.bytes > 8 && !memcmp("OpusHead", op.packet, 8)))
            // {
            //     fprintf(stderr, "Not an ogg opus stream.\n");
            //     exit(1);
            // }

            // /* Parse the header to get stream info */
            // int err = opus_head_parse(&header, op.packet, op.bytes);
            // if (err)
            // {
            //     fprintf(stderr, "Not a ogg opus stream\n");
            //     exit(1);
            // }
            // /* Now we ensure the encoding is correct for Discord */
            // if (header.channel_count != 2 && header.input_sample_rate != 48000)
            // {
            //     fprintf(stderr, "Wrong encoding for Discord, must be 48000Hz sample rate with 2 channels.\n");
            //     exit(1);
            // }

            /* Now loop though all the pages and send the packets to the vc */
            while (ogg_sync_pageout(&oy, &og) == 1)
            {
                while (ogg_stream_check(&os) != 0)
                {
                    ogg_stream_init(&os, ogg_page_serialno(&og));
                }

                if (ogg_stream_pagein(&os, &og) < 0)
                {
                    fprintf(stderr, "Error reading page of Ogg bitstream data.\n");
                    exit(1);
                }

                while (ogg_stream_packetout(&os, &op) != 0)
                {
                    // /* Read remaining headers */
                    // if (op.bytes > 8 && !memcmp("OpusHead", op.packet, 8))
                    // {
                    //     int err = opus_head_parse(&header, op.packet, op.bytes);
                    //     if (err)
                    //     {
                    //         fprintf(stderr, "Not a ogg opus stream\n");
                    //         exit(1);
                    //     }
                    //     if (header.channel_count != 2 && header.input_sample_rate != 48000)
                    //     {
                    //         fprintf(stderr, "Wrong encoding for Discord, must be 48000Hz sample rate with 2 channels.\n");
                    //         exit(1);
                    //     }
                    //     continue;
                    // }
                    // /* Skip the opus tags */
                    // if (op.bytes > 8 && !memcmp("OpusTags", op.packet, 8))
                    //     continue;

                    /* Send the audio */
                    int samples = opus_packet_get_samples_per_frame(op.packet, 48000);

                    v->voiceclient->send_audio_opus(op.packet, op.bytes, samples / 48);
                }
            }

            /* Cleanup */
            fclose(fd);
            ogg_stream_clear(&os);
            ogg_sync_clear(&oy);
            v->voiceclient->insert_marker(fname);
            auto end_time = std::chrono::high_resolution_clock::now();
            auto done = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
            printf("Done streaming for %ld microseconds\n", done.count());
        }
        else throw 1;
    }
}

#endif