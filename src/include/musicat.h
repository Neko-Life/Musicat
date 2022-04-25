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
    auto vector_find(std::vector<T>* _vec, T _find) {
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
            if (mc::vector_find(&joining_list, vc_id) != joining_list.end()) return;
            joining_list.emplace_back(vc_id);
        }

        void remove_joining_vc(dpp::snowflake vc_id) {
            auto i = mc::vector_find(&joining_list, vc_id);
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

    bool has_listener(std::map<dpp::snowflake, dpp::voicestate>* vstate_map) {
        if (vstate_map->size() > 1)
        {
            for (auto r : *vstate_map)
            {
                auto u = dpp::find_user(r.second.user_id);
                if (!u || u->is_bot()) continue;
                return false;
            }
            return true;
        }
        else return false;
    }

    bool has_listener_fetch(dpp::cluster* client, std::map<dpp::snowflake, dpp::voicestate>* vstate_map) {
        if (vstate_map->size() > 1)
        {
            for (auto r : *vstate_map)
            {
                auto u = dpp::find_user(r.second.user_id);
                if (!u)
                {
                    try
                    {
                        printf("Fetching user %ld in vc %ld\n", r.second.user_id, r.second.channel_id);
                        dpp::user_identified uf = client->user_get_sync(r.second.user_id);
                        if (uf.is_bot()) continue;
                    }
                    catch (const dpp::rest_exception* e)
                    {
                        fprintf(stderr, "[ERROR(musicat.182)] Can't fetch user in VC: %ld\n", r.second.user_id);
                        continue;
                    }
                }
                else if (u->is_bot()) continue;
                return false;
            }
            return true;
        }
        else return false;
    }

    class exception {
    private:
        string message;
        int c;

    public:
        exception(const char* _message, int _code = 0) {
            message = string(_message);
            c = _code;
        }

        string what() {
            return message;
        }

        int code() {
            return c;
        }
    };
}

#endif