#ifndef MUSICAT_H
#define MUSICAT_H

#include <dpp/dpp.h>
#include <string>
#include <vector>
#include <stdexcept>

namespace musicat
{
    using string = std::string;

    // Main
    int run(int argc, const char* argv[]);

    template <typename T>
    typename std::vector<T>::iterator vector_find(std::vector<T>* _vec, T _find);

    struct settings
    {
        string defaultPrefix;

        std::map<dpp::snowflake, string> prefix;
        std::vector<dpp::snowflake> joining_list;

        string get_prefix(const dpp::snowflake guildId) const;

        auto set_prefix(const dpp::snowflake guildId, const string newPrefix);

        void set_joining_vc(dpp::snowflake vc_id);

        void remove_joining_vc(dpp::snowflake vc_id);
    };

    /**
     * @brief Destroy and reset connecting state of the guild, must be invoked when failed to join or left a vc
     *
     * @param client The client
     * @param guild_id Guild Id of the vc client connecting in
     */
    void reset_voice_channel(dpp::discord_client* client, dpp::snowflake guild_id);

    /**
     * @brief Get the voice object and connected voice members a vc of a guild
     *
     * @param guild Guild the member in
     * @param user_id Target member
     * @return std::pair<dpp::channel*, std::map<dpp::snowflake, dpp::voicestate>>
     * @throw const char* User isn't in vc
     */
    std::pair<dpp::channel*, std::map<dpp::snowflake, dpp::voicestate>> get_voice(dpp::guild* guild, dpp::snowflake user_id);

    /**
     * @brief Get the voice object and connected voice members a vc of a guild
     *
     * @param guild_id Guild Id the member in
     * @param user_id Target member
     * @return std::pair<dpp::channel*, std::map<dpp::snowflake, dpp::voicestate>>
     * @throw const char* Unknown guild or user isn't in vc
     */
    std::pair<dpp::channel*, std::map<dpp::snowflake, dpp::voicestate>> get_voice_from_gid(dpp::snowflake guild_id, dpp::snowflake user_id);

    /**
     * @brief Execute shell cmd and return anything it printed to console
     *
     * @param cmd Command
     * @return string
     * @throw const char* Exec failed (can't call popen or unknown command)
     */
    string exec(string cmd);

    bool has_listener(std::map<dpp::snowflake, dpp::voicestate>* vstate_map);

    bool has_listener_fetch(dpp::cluster* client, std::map<dpp::snowflake, dpp::voicestate>* vstate_map);

    template<typename T, typename E> void get_inter_param(const E& event, string param_name, T* param)
    {
        auto p = event.get_parameter(param_name);
        if (p.index()) *param = std::get<T>(p);
    }

    class exception : std::exception {
    private:
        const char* message;
        int c;

    public:
        exception(const char* _message, int _code = 0);

        virtual const char* what() const noexcept;

        virtual int code() const noexcept;
    };

    int cli(dpp::cluster& client, dpp::snowflake sha_id, int argc, const char* argv[], bool* running_state = nullptr);

    bool has_permissions(dpp::guild* guild, dpp::user* user, dpp::channel* channel, std::vector<uint64_t> permissions = {});
    bool has_permissions_from_ids(dpp::snowflake guild_id, dpp::snowflake user_id, dpp::snowflake channel_id, std::vector<uint64_t> permissions = {});
}

#endif