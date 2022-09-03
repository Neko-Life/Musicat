#include <dpp/discordclient.h>
#include <mutex>
#include <vector>
#include <chrono>
#include "musicat/cmds.h"
#include "musicat/musicat.h"

namespace musicat
{
    using string = std::string;

    template <typename T>
	typename std::vector<T>::iterator vector_find(std::vector<T>* _vec, T _find) {
	    auto i = _vec->begin();
	    for (; i != _vec->end();i++)
	    {
		if (*i == _find) return i;
	    }
	    return i;
	}

    string settings::get_prefix(const dpp::snowflake guildId) const
    {
	auto gid = prefix.find(guildId);
	if (gid == prefix.end())
	    return defaultPrefix;
	return gid->second;
    }

    auto settings::set_prefix(const dpp::snowflake guildId, const string newPrefix)
    {
	return prefix.insert_or_assign(guildId, newPrefix);
    }

    void settings::set_joining_vc(dpp::snowflake vc_id) {
	if (vector_find(&joining_list, vc_id) != joining_list.end()) return;
	joining_list.emplace_back(vc_id);
    }

    void settings::remove_joining_vc(dpp::snowflake vc_id) {
	auto i = vector_find(&joining_list, vc_id);
	if (i != joining_list.end())
	{
	    joining_list.erase(i);
	    printf("ERASE IS CALLED\n");
	}
    }

    /**
     * @brief Destroy and reset connecting state of the guild, must be invoked when failed to join or left a vc
     *
     * @param client The client
     * @param guild_id Guild Id of the vc client connecting in
     * @param delete_voiceconn Whether to delete found voiceconn, can cause segfault if the underlying structure doesn't exist
     */
    void reset_voice_channel(dpp::discord_client* client, dpp::snowflake guild_id, bool delete_voiceconn) {
	auto v = client->connecting_voice_channels.find(guild_id);
	if (v == client->connecting_voice_channels.end()) return;
	if (v->second && delete_voiceconn)
	{
	    delete v->second;
	    v->second = nullptr;
	}
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
		return true;
	    }
	}
	return false;
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
		return true;
	    }
	}
	return false;
    }

    exception::exception(const char* _message, int _code) {
	message = _message;
	c = _code;
    }

    const char* exception::what() const noexcept {
	return message;
    }

    int exception::code() const noexcept {
	return c;
    }

    bool has_permissions(dpp::guild* guild, dpp::user* user, dpp::channel* channel, std::vector<uint64_t> permissions) {
	if (!guild || !user || !channel) return false;
	uint64_t p = guild->permission_overwrites(guild->base_permissions(user), user, channel);
	for (uint64_t i : permissions) if (!(p & i)) return false;
	return true;
    }

    bool has_permissions_from_ids(dpp::snowflake guild_id, dpp::snowflake user_id, dpp::snowflake channel_id, std::vector<uint64_t> permissions) {
	return has_permissions(dpp::find_guild(guild_id), dpp::find_user(user_id), dpp::find_channel(channel_id), permissions);
    }

    string format_duration(uint64_t dur) {
	uint64_t secondr = dur / 1000;
	uint64_t minuter = secondr / 60;

	uint64_t hour = minuter / 60;
	uint8_t minute = minuter % 60;
	uint8_t second = secondr % 60;
	string ret = "";
	if (hour)
	{
	    string hstr = std::to_string(hour);
	    ret += string(hstr.length() < 2 ? "0" : "") + hstr + ":";
	}
	string mstr = std::to_string(minute);
	string sstr = std::to_string(second);
	ret += string(mstr.length() < 2 ? "0" : "") + mstr + ":" + string(sstr.length() < 2 ? "0" : "") + sstr;
	return ret;
    }

    std::vector<size_t> shuffle_indexes(size_t len) {
	auto r = std::chrono::high_resolution_clock::now();
	srand(r.time_since_epoch().count());
	std::vector<size_t> ret = {};
	ret.reserve(len);
	std::vector<size_t> ori = {};
	ori.reserve(len);
	for (size_t i = 0; i < len; i++) ori.push_back(i);
	auto io = ori.begin();
	while (io != ori.end())
	{
	    int r = rand() % ori.size();
	    auto it = io + r;
	    ret.push_back(*it);
	    ori.erase(it);
	}
	const size_t s = ret.size();
	if (s != len) fprintf(stderr, "[WARN(musicat.209)] Return size %ld != %ld\n", s, len);
	return ret;
    }

    int join_voice(dpp::discord_client* from,
	    player::player_manager_ptr player_manager,
	    const dpp::snowflake& guild_id,
	    const dpp::snowflake& user_id,
	    const dpp::snowflake& sha_id) {

	std::pair<dpp::channel*, std::map<dpp::snowflake, dpp::voicestate>>
	    c, c2;
	// client is connected to a voice channel
	bool has_c2 = false;

	try
	{
	    c = get_voice_from_gid(guild_id, user_id);

	    // no channel cached
	    if (!c.first || !c.first->id) return 3;
	}
	catch (...) {
	    // user isn't in voice channel
	    return 1;
	}

	try
	{
	    c2 = get_voice_from_gid(guild_id, sha_id);
	    // client already in a guild session
	    if (has_listener(&c2.second)) return 2;
	    has_c2 = true;
	}
	catch (...) {}

	if (has_permissions_from_ids(guild_id,
		    sha_id,
		    c.first->id, { dpp::p_view_channel,dpp::p_connect }))
	{
	    {
		std::lock_guard<std::mutex> lk(player_manager->c_m);
		std::lock_guard<std::mutex> lk2(player_manager->wd_m);
		if (has_c2 && c2.first && c2.first->id) {
		    std::lock_guard<std::mutex> lk(player_manager->dc_m);
		    player_manager->disconnecting.insert_or_assign(guild_id, c2.first->id);
		    from->disconnect_voice(guild_id);
		}
		player_manager->connecting.insert_or_assign(guild_id, c.first->id);
		player_manager->waiting_vc_ready.insert_or_assign(guild_id, "2");
	    }

	    std::thread t([player_manager, from, guild_id]() {
		    player_manager->reconnect(from, guild_id);
		    });
	    t.detach();
	    // thread spawned
	    return 0;

	    // no permission
	} else return 4;
    }
}
