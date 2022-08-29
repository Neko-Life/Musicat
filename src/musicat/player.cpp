#include <ogg/ogg.h>
// #include <opus/opusfile.h>
#include <filesystem>
#include <sys/stat.h>
#include <regex>
#include <chrono>
#include <dirent.h>
#include "musicat/player.h"
#include "musicat/cmds.h"

namespace musicat {
    namespace player {
	using string = std::string;

	MCTrack::MCTrack() {}

	MCTrack::MCTrack(YTrack t) {
	    this->raw = t.raw;
	}

	MCTrack::~MCTrack() = default;

	Player::Player(dpp::cluster* _cluster, dpp::snowflake _guild_id) {
	    this->guild_id = _guild_id;
	    this->cluster = _cluster;
	    this->loop_mode = loop_mode_t::l_none;
	    this->shifted_track = 0;
	    this->info_message = nullptr;
	    this->from = nullptr;
	    this->auto_play = false;
	    this->max_history_size = 0;
	    this->stopped = false;
	    this->channel_id = 0;
	}

	Player::~Player() {
	    this->loop_mode = loop_mode_t::l_none;
	    this->shifted_track = 0;
	    this->info_message = nullptr;
	    this->from = nullptr;
	    this->auto_play = false;
	    this->max_history_size = 0;
	    this->stopped = false;
	    this->channel_id = 0;
	};

	Player& Player::add_track(MCTrack track, bool top, dpp::snowflake guild_id, bool update_embed) {
	    size_t siz = 0;
	    {
		if (track.info.raw.is_null())
		{
		    track.info.raw = yt_search::get_track_info(track.url()).audio_info(251).raw;
		}
		std::lock_guard<std::mutex> lk(this->q_m);
		siz = this->queue.size();
		if (top)
		{
		    this->queue.push_front(track);
		    if (this->shifted_track < this->queue.size() - 1)
		    {
			std::lock_guard<std::mutex> lk(this->st_m);
			this->shifted_track++;
		    }
		}
		else this->queue.push_back(track);
	    }
	    if (update_embed && siz > 0UL && guild_id && this->manager) this->manager->update_info_embed(guild_id);
	    return *this;
	}

	Player& Player::set_max_history_size(size_t siz) {
	    this->max_history_size = siz;
	    return *this;
	}

	int Player::skip(dpp::voiceconn* v) const {
	    if (v && v->voiceclient && v->voiceclient->get_secs_remaining() > 0.1)
	    {
		v->voiceclient->pause_audio(false);
		v->voiceclient->skip_to_next_marker();
		return 0;
	    }
	    else return -1;
	}

	Player& Player::set_auto_play(bool state) {
	    this->auto_play = state;
	    return *this;
	}

	bool Player::reset_shifted() {
	    std::lock_guard<std::mutex> lk(this->q_m);
	    std::lock_guard<std::mutex> lk2(this->st_m);
	    if (this->queue.size() && this->shifted_track > 0)
	    {
		auto i = this->queue.begin() + this->shifted_track;
		auto s = *i;
		this->queue.erase(i);
		this->queue.push_front(s);
		this->shifted_track = 0;
		return true;
	    }
	    else return false;
	}

	Player& Player::set_loop_mode(int8_t mode) {
	    loop_mode_t nm = this->loop_mode;
	    switch (mode)
	    {
		case 0: nm = loop_mode_t::l_none; break;
		case 1: nm = loop_mode_t::l_song; break;
		case 2: nm = loop_mode_t::l_queue; break;
		case 3: nm = loop_mode_t::l_song_queue; break;
	    }
	    this->loop_mode = nm;
	    return *this;
	}

	Player& Player::set_channel(dpp::snowflake channel_id) {
	    std::lock_guard<std::mutex> lk(this->ch_m);
	    this->channel_id = channel_id;
	    return *this;
	}

	size_t Player::remove_track(size_t pos, size_t amount) {
	    if (!pos || !amount) return 0;

	    this->reset_shifted();

	    std::lock_guard<std::mutex> lk(this->q_m);
	    size_t siz = this->queue.size();

	    if ((pos + 1) > siz) return 0;
	    size_t max = siz - pos;
	    if (amount > max) amount = max;

	    std::deque<MCTrack>::iterator b = this->queue.begin() + pos;
	    size_t a = 0;

	    while (b != this->queue.end())
	    {
		if (a == amount) break;
		b = this->queue.erase(b);
		a++;
	    }

	    return amount;
	}

	size_t Player::remove_track_by_user(dpp::snowflake user_id) {
	    if (!user_id) return 0;
	    size_t ret = 0;
	    std::lock_guard<std::mutex> lk(this->q_m);
	    auto i = this->queue.begin();
	    while (i != this->queue.end())
	    {
		if (i->user_id == user_id && i != this->queue.begin())
		{
		    this->queue.erase(i);
		    ret++;
		}
		else i++;
	    }
	    return ret;
	}

	bool Player::pause(dpp::discord_client* from, dpp::snowflake user_id) const {
	    auto v = from->get_voice(guild_id);
	    if (v && !v->voiceclient->is_paused())
	    {
		try
		{
		    auto u = get_voice_from_gid(guild_id, user_id);
		    if (u.first->id != v->channel_id) throw exception("You're not in my voice channel", 0);
		}
		catch (const char* e)
		{
		    throw exception("You're not in a voice channel", 1);
		}
		v->voiceclient->pause_audio(true);
		// Paused
		return true;
	    }
	    // Not playing anythin
	    else return false;
	}

	bool Player::shuffle() {
	    size_t siz = 0;
	    {
		std::lock_guard<std::mutex> lk(this->q_m);
		siz = this->queue.size();
		if (siz < 3) return false;
	    }
	    this->reset_shifted();
	    std::deque<MCTrack> n_queue = {};
	    auto b = shuffle_indexes(siz - 1);
	    {
		std::lock_guard<std::mutex> lk(this->q_m);
		MCTrack os = this->queue.at(0);
		this->queue.erase(this->queue.begin());

		for (auto i : b) n_queue.push_back(this->queue.at(i));
		this->queue.clear();

		this->queue = n_queue;
		this->queue.push_front(os);
	    }
	    this->manager->update_info_embed(this->guild_id);
	    return true;
	}

	int Player::seek(int pos, bool abs) {
	    return 0;
	}

	int Player::stop() {
	    return 0;
	}

	int Player::resume() {
	    return 0;
	}

	int Player::search(string query) {
	    return 0;
	}

	int Player::join() {
	    return 0;
	}

	int Player::leave() {
	    return 0;
	}

	int Player::rejoin() {
	    return 0;
	}

	void Player::set_stopped(const bool& val) {
	    std::lock_guard<std::mutex> lk(s_m);
	    this->stopped = val;
	}

	const bool& Player::is_stopped() {
	    std::lock_guard<std::mutex> lk(s_m);
	    return this->stopped;
	}

	Manager::Manager(dpp::cluster* cluster, dpp::snowflake sha_id) {
	    this->cluster = cluster;
	    this->sha_id = sha_id;
	}

	Manager::~Manager() = default;

	std::shared_ptr<Player> Manager::create_player(dpp::snowflake guild_id) {
	    std::lock_guard<std::mutex> lk(this->ps_m);
	    auto l = players.find(guild_id);
	    if (l != players.end()) return l->second;
	    std::shared_ptr<Player> v = std::make_shared<Player>(cluster, guild_id);
	    v->manager = this;
	    players.insert(std::pair(guild_id, v));
	    return v;
	}

	std::shared_ptr<Player> Manager::get_player(dpp::snowflake guild_id) {
	    std::lock_guard<std::mutex> lk(this->ps_m);
	    auto l = players.find(guild_id);
	    if (l != players.end()) return l->second;
	    return NULL;
	}

	void Manager::reconnect(dpp::discord_client* from, dpp::snowflake guild_id) {
	    bool from_dc = false;
	    {
		std::unique_lock<std::mutex> lk(this->dc_m);
		auto a = this->disconnecting.find(guild_id);
		if (a != this->disconnecting.end())
		{
		    from_dc = true;
		    this->dl_cv.wait(lk, [this, &guild_id]() {
			    auto t = this->disconnecting.find(guild_id);
			    return t == this->disconnecting.end();
			    });
		}
	    }
	    {
		std::unique_lock<std::mutex> lk(this->c_m);
		auto a = this->connecting.find(guild_id);
		if (a != this->connecting.end())
		{
		    {
			using namespace std::chrono_literals;
			if (from_dc) std::this_thread::sleep_for(500ms);
		    }
		    from->connect_voice(guild_id, a->second, false, true);
		    this->dl_cv.wait(lk, [this, &guild_id]() {
			    auto t = this->connecting.find(guild_id);
			    return t == this->connecting.end();
			    });
		}
	    }
	}

	bool Manager::delete_player(dpp::snowflake guild_id) {
	    std::lock_guard<std::mutex> lk(this->ps_m);
	    auto l = players.find(guild_id);
	    if (l == players.end()) return false;
	    players.erase(l);
	    return true;
	}

	std::deque<MCTrack> Manager::get_queue(dpp::snowflake guild_id) {
	    auto p = get_player(guild_id);
	    if (!p) return {};
	    p->reset_shifted();
	    return p->queue;
	}

	bool Manager::pause(dpp::discord_client* from, dpp::snowflake guild_id, dpp::snowflake user_id) {
	    auto p = get_player(guild_id);
	    if (!p) return false;
	    bool a = p->pause(from, user_id);
	    if (a)
	    {
		std::lock_guard<std::mutex> lk(mp_m);
		if (vector_find(&this->manually_paused, guild_id) == this->manually_paused.end())
		    this->manually_paused.push_back(guild_id);
		this->update_info_embed(guild_id);
	    }
	    return a;
	}

	bool Manager::unpause(dpp::discord_voice_client* voiceclient, dpp::snowflake guild_id) {
	    std::lock_guard<std::mutex> lk(this->mp_m);
	    auto l = vector_find(&this->manually_paused, guild_id);
	    bool ret;
	    if (l != this->manually_paused.end())
	    {
		this->manually_paused.erase(l);
		ret = true;
	    }
	    else ret = false;
	    voiceclient->pause_audio(false);
	    this->update_info_embed(guild_id);
	    return ret;
	}

	bool Manager::is_disconnecting(dpp::snowflake guild_id) {
	    std::lock_guard<std::mutex> lk1(this->dc_m);
	    return this->disconnecting.find(guild_id) != this->disconnecting.end();
	}

	bool Manager::is_connecting(dpp::snowflake guild_id) {
	    std::lock_guard<std::mutex> lk2(this->c_m);
	    return this->connecting.find(guild_id) != this->connecting.end();
	}

	bool Manager::is_waiting_vc_ready(dpp::snowflake guild_id) {
	    std::lock_guard<std::mutex> lk3(this->wd_m);
	    return this->waiting_vc_ready.find(guild_id) != this->waiting_vc_ready.end();
	}

	bool Manager::voice_ready(dpp::snowflake guild_id, dpp::discord_client* from, dpp::snowflake user_id)
	{
	    bool re = is_connecting(guild_id);
	    if (is_disconnecting(guild_id)
		    || re
		    || is_waiting_vc_ready(guild_id))
	    {
		if (re && from)
		{
		    std::thread t([this, user_id, guild_id](dpp::discord_client* from) {
			    bool user_vc = true;
			    std::pair<dpp::channel*, std::map<dpp::snowflake, dpp::voicestate>> uservc;
			    try
			    {
			    uservc = get_voice_from_gid(guild_id, user_id);
			    }
			    catch (...)
			    {
			    user_vc = false;
			    }
			    try
			    {
			    auto f = from->connecting_voice_channels.find(guild_id);
			    auto c = get_voice_from_gid(guild_id, from->creator->me.id);
			    if (f == from->connecting_voice_channels.end() || !f->second)
			    {
			    std::lock_guard<std::mutex> lk(this->dc_m);
			    this->disconnecting[guild_id] = 1;
			    from->disconnect_voice(guild_id);
			    }
			    else if (user_vc && uservc.first->id != c.first->id)
			    {
				printf("Disconnecting as it seems I just got moved to different vc and connection not updated yet: %ld\n", guild_id);
				std::lock_guard<std::mutex> lk(this->dc_m);
				std::lock_guard<std::mutex> lk2(this->c_m);
				this->disconnecting[guild_id] = f->second->channel_id;
				this->connecting[guild_id] = uservc.first->id;
				from->disconnect_voice(guild_id);
			    }
			    }
			    catch (...)
			    {
				reset_voice_channel(from, guild_id);

				if (user_id && user_vc) try
				{
				    std::lock_guard<std::mutex> lk(this->c_m);
				    auto p = this->connecting.find(guild_id);
				    std::map<dpp::snowflake, dpp::voicestate> vm = {};
				    if (p != this->connecting.end())
				    {
					auto gc = dpp::find_channel(p->second);
					if (gc) vm = gc->get_voice_members();
					auto l = has_listener(&vm);
					if (!l && p->second != uservc.first->id) p->second = uservc.first->id;
				    }
				}
				catch (...) {}
			    }
			    this->reconnect(from, guild_id);
		    }, from);
		    t.detach();
		}
		return false;
	    }
	    return true;
	}

	void Manager::stop_stream(dpp::snowflake guild_id) {
	    {
		std::lock_guard<std::mutex> lk(this->sq_m);
		if (vector_find(&this->stop_queue, guild_id) == this->stop_queue.end())
		    this->stop_queue.push_back(guild_id);
	    }
	}

	int Manager::skip(dpp::voiceconn* v, dpp::snowflake guild_id, dpp::snowflake user_id, int64_t amount) {
	    if (!v) return -1;
	    auto p = get_player(guild_id);
	    if (!p) return -1;
	    try
	    {
		auto u = get_voice_from_gid(guild_id, user_id);
		if (u.first->id != v->channel_id) throw exception("You're not in my voice channel", 0);

		unsigned siz = 0;
		for (auto& i : u.second)
		{
		    auto& a = i.second;
		    if (a.is_deaf() || a.is_self_deaf()) continue;
		    auto user = dpp::find_user(a.user_id);
		    if (user->is_bot()) continue;
		    siz++;
		}
		// if (siz > 1U)
		// {
		//     std::lock_guard<std::mutex> lk(p->q_m);
		//     auto& track = p->queue.at(0);
		//     if (track.user_id != user_id && track.user_id != this->sha_id)
		//     {
		//         amount = 1;
		//         bool exist = false;
		//         for (const auto& i : track.skip_vote)
		//         {
		//             if (i == user_id)
		//             {
		//                 exist = true;
		//                 break;
		//             }
		//         }
		//         if (!exist)
		//         {
		//             track.skip_vote.push_back(user_id);
		//         }

		//         unsigned ts = siz / 2U + 1U;
		//         size_t ret = track.skip_vote.size();
		//         if (ret < ts) return (int)ret;
		//         else track.skip_vote.clear();
		//     }
		//     else if (amount > 1)
		//     {
		//         int64_t count = 0;
		//         for (const auto& t : p->queue)
		//         {
		//             if (t.user_id == user_id || t.user_id == this->sha_id) count++;
		//             else break;

		//             if (amount == count) break;
		//         }
		//         if (amount > count) amount = count;
		//     }
		// }
	    }
	    catch (const char* e)
	    {
		throw exception("You're not in a voice channel", 1);
	    }
	    auto siz = p->queue.size();
	    if (amount < (siz || 1)) amount = siz || 1;
	    if (amount > 1000) amount = 1000;
	    const bool l_s = p->loop_mode == loop_mode_t::l_song_queue;
	    const bool l_q = p->loop_mode == loop_mode_t::l_queue;
	    {
		std::lock_guard<std::mutex> lk(p->q_m);
		for (int64_t i =
			(p->loop_mode == loop_mode_t::l_song || l_s)
			? 0 : 1;
			i < amount; i++)
		{
		    if (p->queue.begin() == p->queue.end()) break;
		    auto l = p->queue.front();
		    p->queue.pop_front();
		    if (l_s || l_q) p->queue.push_back(l);
		}
	    }
	    if (v && v->voiceclient && v->voiceclient->get_secs_remaining() > 0.1)
		this->stop_stream(guild_id);
	    int a = p->skip(v);
	    return a;
	}

	void Manager::download(string fname, string url, dpp::snowflake guild_id) {
	    std::thread tj([this](string fname, string url, dpp::snowflake guild_id) {
		    {
		    std::lock_guard<std::mutex> lk(this->dl_m);
		    this->waiting_file_download[fname] = guild_id;
		    }
		    {
		    struct stat buf;
		    if (stat("music", &buf) != 0)
		    std::filesystem::create_directory("music");
		    }
		    string cmd = string("yt-dlp -f 251 --http-chunk-size 2M -o - '") + url
		    + string("' 2>/dev/null | ffmpeg -i - -b:a 384000 -ar 48000 -ac 2 -sn -vn -c libopus -f ogg 'music/")
		    + std::regex_replace(
			    fname, std::regex("(')"), "'\\''",
			    std::regex_constants::match_any)
		    + string("'");
		    printf("DOWNLOAD: \"%s\" \"%s\"\n", fname.c_str(), url.c_str());
		    printf("CMD: %s\n", cmd.c_str());
		    FILE* a = popen(cmd.c_str(), "w");
		    pclose(a);
		    {
			std::lock_guard<std::mutex> lk(this->dl_m);
			this->waiting_file_download.erase(fname);
		    }
		    this->dl_cv.notify_all();
	    }, fname, url, guild_id);
	    tj.detach();
	}

	void Manager::wait_for_download(string file_name) {
	    std::unique_lock<std::mutex> lk(this->dl_m);
	    this->dl_cv.wait(lk, [this, file_name]() {
		    return this->waiting_file_download.find(file_name) == this->waiting_file_download.end();
		    });
	}

	void Manager::stream(dpp::discord_voice_client* v, string fname) {
	    dpp::snowflake server_id;
	    std::chrono::_V2::system_clock::time_point start_time;
	    if (v && !v->terminating && v->is_ready())
	    {
		FILE* fd;
		ogg_sync_state oy;
		ogg_stream_state os;
		try
		{
		    server_id = v->server_id;
		    {
			struct stat buf;
			if (stat("music", &buf) != 0)
			{
			    std::filesystem::create_directory("music");
			    throw 2;
			}
		    }
		    start_time = std::chrono::high_resolution_clock::now();
		    printf("Streaming \"%s\" to %ld\n", fname.c_str(), server_id);
		    
		    fd = fopen((string("music/") + fname).c_str(), "rb");
		    if (!fd) throw 2;

		    printf("Initializing buffer\n");
		    ogg_page og;
		    ogg_packet op;
		    // OpusHead header;
		    char* buffer;

		    fseek(fd, 0L, SEEK_END);
		    size_t sz = ftell(fd);
		    printf("SIZE_T: %ld\n", sz);
		    rewind(fd);

		    ogg_sync_init(&oy);

		    // int eos = 0;
		    // int i;

		    buffer = ogg_sync_buffer(&oy, sz);
		    fread(buffer, 1, sz, fd);
		    fclose(fd);

		    bool no_prob = true;

		    ogg_sync_wrote(&oy, sz);

		    if (ogg_sync_pageout(&oy, &og) != 1)
		    {
			fprintf(stderr, "Does not appear to be ogg stream.\n");
			no_prob = false;
		    }

		    ogg_stream_init(&os, ogg_page_serialno(&og));

		    if (ogg_stream_pagein(&os, &og) < 0)
		    {
			fprintf(stderr, "Error reading initial page of ogg stream.\n");
			no_prob = false;
		    }

		    if (ogg_stream_packetout(&os, &op) != 1)
		    {
			fprintf(stderr, "Error reading header packet of ogg stream.\n");
			no_prob = false;
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
		    if (no_prob) while (ogg_sync_pageout(&oy, &og) == 1)
		    {
			{
			    std::lock_guard<std::mutex> lk(this->sq_m);
			    auto sq = vector_find(&this->stop_queue, server_id);
			    if (sq != this->stop_queue.end()) break;
			}
			if (!v || v->terminating)
			{
			    fprintf(stderr, "[ERROR(sha_player.431)] Can't continue streaming, connection broken\n");
			    break;
			}
			while (ogg_stream_check(&os) != 0)
			{
			    ogg_stream_init(&os, ogg_page_serialno(&og));
			}

			if (ogg_stream_pagein(&os, &og) < 0)
			{
			    fprintf(stderr, "Error reading page of Ogg bitstream data.\n");
			    break;
			}

			int res;

			while ((res = ogg_stream_packetout(&os, &op)) != 0)
			{
			    {
				std::lock_guard<std::mutex> lk(this->sq_m);
				auto sq = vector_find(&this->stop_queue, server_id);
				if (sq != this->stop_queue.end()) break;
			    }
			    if (res < 1)
			    {
				ogg_stream_pagein(&os, &og);
				continue;
			    }
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
			    /* Skip the opus tags */
			    // if (op.bytes > 8 && !memcmp("OpusTags", op.packet, 8))
			    //     continue;

			    if (!v || v->terminating)
			    {
				fprintf(stderr, "[ERROR(sha_player.382)] Can't continue streaming, connection broken\n");
				break;
			    }

			    /* Send the audio */
			    // int samples = opus_packet_get_samples_per_frame(op.packet, 48000);

			    if (v && !v->terminating) v->send_audio_opus(op.packet, op.bytes, 20);//samples / 48);

			    bool br = v->terminating;

			    while (v && !v->terminating && v->get_secs_remaining() > 0.5)
			    {
				std::this_thread::sleep_for(std::chrono::milliseconds(250));
				if (v->terminating) br = true;
			    }

			    {
				std::lock_guard<std::mutex> lk(this->sq_m);
				auto sq = vector_find(&this->stop_queue, server_id);
				if (sq != this->stop_queue.end())
				{
				    br = true;
				    break;
				}
			    }

			    if (br)
			    {
				printf("Breaking\n");
				break;
			    }
			}
		    }
		}
		catch (const std::system_error& e)
		{
		    fprintf(stderr, "[ERROR(player.553)] %d: %s\n", e.code().value(), e.what());
		}
		/* Cleanup */
		ogg_stream_clear(&os);
		ogg_sync_clear(&oy);
		auto end_time = std::chrono::high_resolution_clock::now();
		auto done = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
		printf("Done streaming for %ld milliseconds\n", done.count());
	    }
	    else throw 1;
	}

	void Manager::play(dpp::discord_voice_client* v, string fname, dpp::snowflake channel_id, bool notify_error) {
	    std::thread tj([this](dpp::discord_voice_client* v, string fname, dpp::snowflake channel_id, bool notify_error) {
		    printf("Attempt to stream\n");
		    auto server_id = v->server_id;
		    auto voice_channel_id = v->channel_id;
		    try
		    {
		    this->stream(v, fname);
		    }
		    catch (int e)
		    {
		    printf("ERROR_CODE: %d\n", e);
		    if (notify_error)
		    {
		    string msg = "";
		    // Maybe connect/reconnect here if there's connection error
		    if (e == 2) msg = "Can't start playback";
		    else if (e == 1) msg = "No connection";
		    dpp::message m;
		    m.set_channel_id(channel_id)
		    .set_content(msg);
		    this->cluster->message_create(m);
		    }
		    }
		    if (server_id)
		    {
			std::lock_guard<std::mutex> lk(this->sq_m);
			auto sq = vector_find(&this->stop_queue, server_id);
			if (sq != this->stop_queue.end())
			{
			    printf("ERASING QUEUE\n");
			    this->stop_queue.erase(sq);
			    this->stop_queue_cv.notify_all();
			}
		    }
		    if (v && !v->terminating) v->insert_marker("e");
		    else
		    {
			try
			{
			    get_voice_from_gid(server_id, this->sha_id);
			    return;
			}
			catch (...)
			{
			}
			if (server_id && voice_channel_id)
			{
			    std::lock_guard<std::mutex> lk(this->c_m);
			    this->connecting[server_id] = voice_channel_id;
			}
			// if (v) v->~discord_voice_client();
		    }
	    }, v, fname, channel_id, notify_error);
	    tj.detach();
	}

	bool Manager::send_info_embed(dpp::snowflake guild_id, bool update, bool force_playing_status) {
	    {
		auto player = this->get_player(guild_id);
		if (!player) throw exception("No player");

		if (update && !player->info_message)
		{
		    printf("No message\n");
		    return false;
		}

		auto channel_id = player->channel_id;

		auto g = dpp::find_guild(guild_id);
		auto c = dpp::find_channel(channel_id);
		bool embed_perms = has_permissions(g, &this->cluster->me, c, { dpp::p_view_channel,dpp::p_send_messages,dpp::p_embed_links });
		bool view_history_perm = has_permissions(g, &this->cluster->me, c, { dpp::p_read_message_history });

		bool delete_original = false;
		if (update)
		{
		    if (!view_history_perm || (player->info_message && player->info_message->is_source_message_deleted()))
		    {
			update = false;
			delete_original = true;
		    }
		}
		else
		{
		    if (!embed_perms) throw exception("No permission");
		}

		dpp::embed e;
		try
		{
		    e = get_playing_info_embed(guild_id, force_playing_status);
		}
		catch (const exception& e)
		{
		    fprintf(stderr, "[ERROR(player.646)] Failed to get_playing_info_embed: %s\n", e.what());
		    return false;
		}
		catch (const dpp::exception& e)
		{
		    fprintf(stderr, "[ERROR(player.646)] Failed to get_playing_info_embed [dpp::exception]: %s\n", e.what());
		    return false;
		}
		catch (const std::logic_error& e)
		{
		    fprintf(stderr, "[ERROR(player.646)] Failed to get_playing_info_embed [std::logic_error]: %s\n", e.what());
		    return false;
		}

		auto m_cb = [this, player, guild_id, channel_id](dpp::confirmation_callback_t cb) {
		    if (cb.is_error())
		    {
			fprintf(stderr, "[ERROR(player.668)] message_create callback error:\nmes: %s\ncode: %d\nerr:\n", cb.get_error().message.c_str(), cb.get_error().code);
			for (const auto& i : cb.get_error().errors)
			    fprintf(stderr, "c: %s\nf: %s\no: %s\nr: %s\n",
				    i.code.c_str(),
				    i.field.c_str(),
				    i.object.c_str(),
				    i.reason.c_str()
				   );
			return;
		    }
		    if (cb.value.index())
		    {
			if (!player)
			{
			    fprintf(stderr, "PLAYER GONE WTFF\n");
			    return;
			}

			std::lock_guard<std::mutex> lk(this->imc_m);
			if (player->info_message)
			{
			    auto id = player->info_message->id;
			    this->info_messages_cache.erase(id);
			}

			player->info_message = std::make_shared<dpp::message>(std::get<dpp::message>(cb.value));
			this->info_messages_cache[player->info_message->id] = player->info_message;
			printf("New message info: %ld\n", player->info_message->id);
		    }
		    else printf("No message_create cb size\n");
		};

		if (delete_original)
		{
		    this->delete_info_embed(guild_id);
		}

		if (!update)
		{
		    dpp::message m;
		    m.add_embed(e)
			.set_channel_id(channel_id);

		    this->cluster->message_create(m, m_cb);
		}
		else
		{
		    auto mn = *player->info_message;
		    mn.embeds.pop_back();
		    mn.embeds.push_back(e);
		    printf("Channel Info Embed Id Edit: %ld %ld\n", mn.channel_id, mn.id);
		    this->cluster->message_edit(mn, m_cb);
		}
		return true;
	    }
	}

	bool Manager::update_info_embed(dpp::snowflake guild_id, bool force_playing_status) {
	    printf("Update info called\n");
	    return this->send_info_embed(guild_id, true, force_playing_status);
	}

	bool Manager::delete_info_embed(dpp::snowflake guild_id, dpp::command_completion_event_t callback) {
	    auto player = this->get_player(guild_id);
	    if (!player) return false;

	    auto retdel = [player, this]() {
		std::lock_guard<std::mutex> lk(this->imc_m);
		auto id = player->info_message->id;
		this->info_messages_cache.erase(id);
		return true;
	    };

	    if (!player->info_message) return false;
	    else if (player->info_message->is_source_message_deleted()) return retdel();

	    auto mid = player->info_message->id;
	    auto cid = player->info_message->channel_id;

	    printf("Channel Info Embed Id Delete: %ld\n", cid);
	    this->cluster->message_delete(mid, cid, callback);
	    return retdel();
	}

	bool Manager::handle_on_track_marker(const dpp::voice_track_marker_t& event, std::shared_ptr<Manager> shared_manager) {
	    printf("Handling voice marker: \"%s\" in guild %ld\n", event.track_meta.c_str(), event.voice_client->server_id);

	    {
		std::lock_guard<std::mutex> lk(this->mp_m);
		auto l = vector_find(&this->manually_paused, event.voice_client->server_id);
		if (l != this->manually_paused.end())
		{
		    this->manually_paused.erase(l);
		}
	    }

	    if (!event.voice_client) { printf("NO CLIENT\n"); return false; }
	    if (this->is_disconnecting(event.voice_client->server_id))
	    {
		printf("DISCONNECTING\n");
		return false;
	    }
	    auto p = this->get_player(event.voice_client->server_id);
	    if (!p) { printf("NO PLAYER\n"); return false; }
	    MCTrack s;
	    {
		{
		    std::lock_guard<std::mutex> lk(p->q_m);
		    if (p->queue.size() == 0)
		    {
			printf("NO SIZE BEFORE: %d\n", p->loop_mode);
			return false;
		    }
		}

		// Handle shifted tracks (tracks shifted to the front of the queue)
		printf("Resetting shifted: %d\n", p->reset_shifted());
		std::lock_guard<std::mutex> lk(p->q_m);

		// Do stuff according to loop mode when playback ends
		if (event.track_meta == "e" && !p->is_stopped())
		{
		    if (p->loop_mode == loop_mode_t::l_none) p->queue.pop_front();
		    else if (p->loop_mode == loop_mode_t::l_queue)
		    {
			auto l = p->queue.front();
			p->queue.pop_front();
			p->queue.push_back(l);
		    }
		}
		if (p->queue.size() == 0)
		{
		    printf("NO SIZE AFTER: %d\n", p->loop_mode);
		    return false;
		}
		p->queue.front().skip_vote.clear();
		s = p->queue.front();
		p->set_stopped(false);
	    }

	    try
	    {
		auto c = get_voice_from_gid(event.voice_client->server_id, this->sha_id);
		if (!has_listener(&c.second)) return false;
	    }
	    catch (...) {}

	    if (event.voice_client && event.voice_client->get_secs_remaining() < 0.1)
	    {
		std::thread tj([this, shared_manager](dpp::discord_voice_client* v, MCTrack track, string meta, std::shared_ptr<Player> player) {
			bool timed_out = false;
			auto guild_id = v->server_id;
			dpp::snowflake channel_id = player->channel_id;
			// std::thread tmt([this](bool* _v) {
			//     int _w = 30;
			//     while (_v && *_v == false && _w > 0)
			//     {
			//         sleep(1);
			//         --_w;
			//     }
			//     if (_w) return;
			//     if (_v)
			//     {
			//         *_v = true;
			//         this->dl_cv.notify_all();
			//     }
			// }, &timed_out);
			// tmt.detach();
			{
			std::unique_lock<std::mutex> lk(this->wd_m);
			auto a = this->waiting_vc_ready.find(guild_id);
			if (a != this->waiting_vc_ready.end())
			{
			    printf("Waiting for ready state\n");
			    this->dl_cv.wait(lk, [this, &guild_id, &timed_out]() {
				    auto t = this->waiting_vc_ready.find(guild_id);
				    // if (timed_out)
				    // {
				    //     this->waiting_vc_ready.erase(t);
				    //     return true;
				    // }
				    auto c = t == this->waiting_vc_ready.end();
				    return c;
				    });
			}
			}
			{
			    std::unique_lock<std::mutex> lk(this->dl_m);
			    auto a = this->waiting_file_download.find(track.filename);
			    if (a != this->waiting_file_download.end())
			    {
				printf("Waiting for download\n");
				this->dl_cv.wait(lk, [this, track, &timed_out]() {
					auto t = this->waiting_file_download.find(track.filename);
					// if (timed_out)
					// {
					//     this->waiting_file_download.erase(t);
					//     return true;
					// }
					auto c = t == this->waiting_file_download.end();
					return c;
					});
			    }
			}
			string id = track.id();
			if (player->auto_play)
			{
			    printf("Getting new autoplay track: %s\n", id.c_str());
			    command::play::add_track(true,
				    v->server_id, string(
					"https://www.youtube.com/watch?v="
					) + id + "&list=RD" + id,
				    0,
				    true,
				    NULL,
				    0,
				    this->sha_id,
				    shared_manager,
				    false,
				    player->from);
			}
			{
			    std::lock_guard<std::mutex> lk(player->h_m);
			    if (player->max_history_size)
			    {
				player->history.push_back(id);
				while (player->history.size() > player->max_history_size)
				{
				    player->history.pop_front();
				}
			    }
			}
			auto c = dpp::find_channel(channel_id);
			auto g = dpp::find_guild(guild_id);
			bool embed_perms = has_permissions(g, &this->cluster->me, c, { dpp::p_view_channel,dpp::p_send_messages,dpp::p_embed_links });
			{
			    std::ifstream test(string("music/") + track.filename, std::ios_base::in | std::ios_base::binary);
			    if (!test.is_open())
			    {
				if (v && !v->terminating) v->insert_marker("e");
				if (embed_perms)
				{
				    dpp::message m;
				    m.set_channel_id(channel_id)
					.set_content("Can't play track: " + track.title() + " (added by <@" + std::to_string(track.user_id) + ">)");
				    this->cluster->message_create(m);
				}
				return;
			    }
			    else test.close();
			}
			if (timed_out) throw exception("Operation took too long, aborted...", 0);
			if (meta == "r") v->send_silence(60);

			// Send play info embed
			try
			{
			    this->play(v, track.filename, channel_id, embed_perms);
			    if (embed_perms)
			    {
				// Update if last message is the info embed message
				if (c && player->info_message && c->last_message_id && c->last_message_id == player->info_message->id)
				{
				    if (player->loop_mode != loop_mode_t::l_song && player->loop_mode != loop_mode_t::l_song_queue)
					this->update_info_embed(guild_id, true);
				}
				else
				{
				    this->delete_info_embed(guild_id);
				    this->send_info_embed(guild_id, false, true);
				}
			    }
			    else fprintf(stderr, "[EMBED_UPDATE] No channel or permission to send info embed\n");
			}
			catch (const exception& e)
			{
			    fprintf(stderr, "[ERROR(player.646)] %s\n", e.what());
			    auto cd = e.code();
			    if (cd == 1 || cd == 2)
				if (embed_perms)
				{
				    dpp::message m;
				    m.set_channel_id(channel_id)
					.set_content(e.what());
				    this->cluster->message_create(m);
				}
			}
		}, event.voice_client, s, event.track_meta, p);
		tj.detach();
		return true;
	    }
	    else printf("TRACK SIZE\n");
	    return false;
	}

	dpp::embed Manager::get_playing_info_embed(dpp::snowflake guild_id, bool force_playing_status) {
	    auto player = this->get_player(guild_id);
	    if (!player) throw exception("No player");

	    // Reset shifted tracks
	    player->reset_shifted();
	    MCTrack track;
	    MCTrack prev_track;
	    MCTrack next_track;
	    MCTrack skip_track;

	    {
		std::lock_guard<std::mutex> lk(player->q_m);
		auto siz = player->queue.size();
		if (!siz) throw exception("No track");
		track = player->queue.front();

		prev_track = player->queue.at(siz - 1UL);

		auto lm = player->loop_mode;
		if (lm == loop_mode_t::l_queue) next_track = (siz == 1UL) ? track : player->queue.at(1);
		else if (lm == loop_mode_t::l_none)
		{
		    if (siz > 1UL) next_track = player->queue.at(1);
		}
		else
		{
		    // if (loop mode == one | one/queue)
		    next_track = track;
		    if (siz > 1UL) skip_track = player->queue.at(1);
		}
	    }

	    dpp::guild_member o = dpp::find_guild_member(guild_id, this->cluster->me.id);
	    dpp::guild_member u = dpp::find_guild_member(guild_id, track.user_id);
	    dpp::user* uc = dpp::find_user(u.user_id);
	    uint32_t color = 0;
	    for (auto i : o.roles)
	    {
		auto r = dpp::find_role(i);
		if (!r || !r->colour) continue;
		color = r->colour;
		break;
	    }
	    string eaname = u.nickname.length() ? u.nickname : uc->username;
	    dpp::embed_author ea;
	    ea.name = eaname;
	    auto ua = u.get_avatar_url(4096);
	    auto uca = uc->get_avatar_url(4096);
	    if (ua.length()) ea.icon_url = ua;
	    else if (uca.length()) ea.icon_url = uca;

	    static const char* l_mode[] = { "Repeat one", "Repeat queue", "Repeat one/queue" };
	    static const char* p_mode[] = { "Paused", "Playing" };
	    string et = track.bestThumbnail().url;
	    dpp::embed e;
	    e.set_description(track.snippetText())
		.set_title(track.title())
		.set_url(track.url())
		.set_author(ea);

	    if (!prev_track.raw.is_null()) e.add_field("PREVIOUS", "[" + prev_track.title() + "](" + prev_track.url() + ")", true);
	    if (!next_track.raw.is_null()) e.add_field("NEXT", "[" + next_track.title() + "](" + next_track.url() + ")", true);
	    if (!skip_track.raw.is_null()) e.add_field("SKIP", "[" + skip_track.title() + "](" + skip_track.url() + ")");

	    string ft = "";

	    bool tinfo = !track.info.raw.is_null();

	    if (tinfo)
	    {
		ft += format_duration(track.info.duration());
	    }

	    bool has_p = false;

	    if (player->from)
	    {
		auto con = player->from->get_voice(guild_id);
		if (con && con->voiceclient)
		    if (con->voiceclient->get_secs_remaining() > 0.1)
		    {
			has_p = true;
			if (ft.length()) ft += " | ";
			if (con->voiceclient->is_paused()) ft += p_mode[0];
			else ft += p_mode[1];
		    }
	    }
	    if (force_playing_status && !has_p)
	    {
		if (ft.length()) ft += " | ";
		ft += p_mode[1];
	    }

	    if (player->loop_mode)
	    {
		if (ft.length()) ft += " | ";
		ft += l_mode[player->loop_mode - 1];
	    }
	    if (player->auto_play)
	    {
		if (ft.length()) ft += " | ";
		ft += "Autoplay";
		if (player->max_history_size) ft += string(" (") + std::to_string(player->max_history_size) + ")";
	    }
	    if (tinfo)
	    {
		if (ft.length()) ft += " | ";
		ft += string("[") + std::to_string(track.info.average_bitrate()) + "]";
	    }
	    if (ft.length()) e.set_footer(ft, "");
	    if (color)
		e.set_color(color);
	    if (et.length())
		e.set_image(et);
	    return e;
	}

	std::vector<std::string> Manager::get_available_tracks(const size_t amount) const {
	    std::vector<std::string> ret = {};
	    size_t c = 0;
	    auto dir = opendir("./music");

	    if (dir != NULL)
	    {
		auto file = readdir(dir);
		while (file != NULL)
		{
		    if (file->d_type == DT_REG)
		    {
			string s = string(file->d_name);
			ret.push_back(s.substr(0, s.length() - 4));
		    }

		    if (amount && ++c == amount) break;
		    file = readdir(dir);
		}
		closedir(dir);
	    }
	    return ret;
	}

	void Manager::handle_on_voice_ready(const dpp::voice_ready_t& event) {
	    {
		std::lock_guard<std::mutex> lk(this->wd_m);
		printf("on_voice_ready\n");
		if (this->waiting_vc_ready.find(event.voice_client->server_id) != this->waiting_vc_ready.end())
		{
		    this->waiting_vc_ready.erase(event.voice_client->server_id);
		    this->dl_cv.notify_all();
		}
	    }
	    {
		{
		    std::lock_guard<std::mutex> lk(this->c_m);
		    if (this->connecting.find(event.voice_client->server_id) != this->connecting.end())
		    {
			this->connecting.erase(event.voice_client->server_id);
			this->dl_cv.notify_all();
		    }
		}
	    }
	    auto i = event.voice_client->get_tracks_remaining();
	    auto l = event.voice_client->get_secs_remaining();
	    printf("TO INSERT %d::%f\n", i, l);
	    if (l < 0.1)
	    {
		event.voice_client->insert_marker("r");
		printf("INSERTED\n");
	    }
	}

	void Manager::handle_on_voice_state_update(const dpp::voice_state_update_t& event) {
	    // Non client's user code
	    if (event.state.user_id != sha_id)
	    {
		dpp::voiceconn* v = event.from->get_voice(event.state.guild_id);

		if (!v || !v->channel_id || !v->voiceclient) return;

		// Pause audio when no user listening in vc
		if (v->channel_id != event.state.channel_id
			&& v->voiceclient->get_secs_remaining() > 0.1
			&& !v->voiceclient->is_paused())
		{
		    std::lock_guard<std::mutex> lk(this->mp_m);
		    if (event.from
			    && vector_find(&this->manually_paused, event.state.guild_id) == this->manually_paused.end())
		    {
			try
			{
			    auto voice = get_voice_from_gid(event.state.guild_id, sha_id);
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
				this->update_info_embed(event.state.guild_id);
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
		    if (v->channel_id == event.state.channel_id
			    && v->voiceclient->get_secs_remaining() > 0.1
			    && v->voiceclient->is_paused())
		    {
			std::lock_guard<std::mutex> lk(this->mp_m);
			// Whether the track paused automatically
			if (vector_find(&this->manually_paused, event.state.guild_id) == this->manually_paused.end())
			{
			    std::thread tj([this, event](dpp::discord_voice_client* vc) {
				    std::this_thread::sleep_for(std::chrono::milliseconds(2500));
				    try
				    {
				    auto a = get_voice_from_gid(event.state.guild_id, event.state.user_id);
				    if (vc && !vc->terminating && a.first && a.first->id == event.state.channel_id)
				    {
				    vc->pause_audio(false);
				    this->update_info_embed(event.state.guild_id);
				    }
				    }
				    catch (...) {}
				    }, v->voiceclient);
			    tj.detach();
			}
		    }
		    else if (event.state.channel_id && v->channel_id != event.state.channel_id)
		    {
			try
			{
			    auto m = get_voice_from_gid(event.state.guild_id, this->sha_id);
			    if (has_listener(&m.second))
			    {
				std::lock_guard<std::mutex> lk(this->dc_m);
				std::lock_guard<std::mutex> lk2(this->c_m);
				std::lock_guard<std::mutex> lk3(this->wd_m);
				this->disconnecting[event.state.guild_id] = v->channel_id;
				this->connecting[event.state.guild_id] = event.state.channel_id;
				this->waiting_vc_ready[event.state.guild_id] = "0";
				event.from->disconnect_voice(event.state.guild_id);
			    }
			    std::thread tj([this, event]() {
				    this->reconnect(event.from, event.state.guild_id);
				    });
			    tj.detach();
			}
			catch (...) {}
		    }
		}

		// End non client's user code
		return;
	    }

	    // Client user code -----------------------------------

	    if (!event.state.channel_id)
	    {
		{
		    std::lock_guard<std::mutex> lk(this->dc_m);
		    printf("on_voice_state_leave\n");
		    if (this->disconnecting.find(event.state.guild_id) != this->disconnecting.end())
		    {
			this->disconnecting.erase(event.state.guild_id);
			this->dl_cv.notify_all();
		    }
		}
	    }
	    else
	    {
		{
		    std::lock_guard<std::mutex> lk(this->c_m);
		    printf("on_voice_state_join\n");
		    if (this->connecting.find(event.state.guild_id) != this->connecting.end())
		    {
			this->connecting.erase(event.state.guild_id);
			this->dl_cv.notify_all();
		    }
		}

		try
		{
		    auto a = get_voice_from_gid(event.state.guild_id, event.state.user_id);
		    auto v = event.from->get_voice(event.state.guild_id);
		    if (v && v->channel_id && v->channel_id != a.first->id)
		    {
			this->stop_stream(event.state.guild_id);

			if (v->voiceclient && !v->voiceclient->terminating)
			{
			    v->voiceclient->pause_audio(false);
			    v->voiceclient->skip_to_next_marker();
			}

			{
			    std::lock_guard<std::mutex> lk(this->dc_m);
			    this->disconnecting[event.state.guild_id] = v->channel_id;
			    event.from->disconnect_voice(event.state.guild_id);
			}

			if (has_listener(&a.second))
			{
			    std::lock_guard<std::mutex> lk(this->c_m);
			    std::lock_guard<std::mutex> lk2(this->wd_m);
			    this->connecting.insert_or_assign(event.state.guild_id, a.first->id);
			    this->waiting_vc_ready[event.state.guild_id] = "0";
			}

			std::thread tj([this, event]() {
				std::this_thread::sleep_for(std::chrono::seconds(1));
				this->voice_ready(event.state.guild_id, event.from);
				});
			tj.detach();
		    }
		}
		catch (...) {}
	    }
	    // if (muted) player_manager->pause(event.guild_id);
	    // else player_manager->resume(guild_id);
	}

	bool Manager::set_info_message_as_deleted(dpp::snowflake id) {
	    std::lock_guard<std::mutex> lk(this->imc_m);
	    auto m = this->info_messages_cache.find(id);
	    if (m != this->info_messages_cache.end())
	    {
		if (!m->second->is_source_message_deleted())
		{
		    m->second->set_flags(m->second->flags | dpp::m_source_message_deleted);
		    return true;
		}
	    }
	    return false;
	}

	void Manager::handle_on_message_delete(const dpp::message_delete_t& event) {
	    this->set_info_message_as_deleted(event.deleted->id);
	}

	void Manager::handle_on_message_delete_bulk(const dpp::message_delete_bulk_t& event) {
	    for (auto i : event.deleted) this->set_info_message_as_deleted(i);
	}

	size_t Manager::remove_track(dpp::snowflake guild_id, size_t pos, size_t amount) {
	    auto p = this->get_player(guild_id);
	    if (!p) return 0;
	    return p->remove_track(pos, amount);
	}

	bool Manager::shuffle_queue(dpp::snowflake guild_id) {
	    auto p = this->get_player(guild_id);
	    if (!p) return false;
	    return p->shuffle();
	}

	void Manager::set_ignore_marker(const dpp::snowflake& guild_id) {
	    std::lock_guard<std::mutex> lk(this->im_m);
	    auto l = vector_find(&this->ignore_marker, guild_id);
	    if (l == this->ignore_marker.end())
	    {
		this->ignore_marker.push_back(guild_id);
	    }
	}

	void Manager::remove_ignore_marker(const dpp::snowflake& guild_id) {
	    std::lock_guard<std::mutex> lk(this->im_m);
	    auto l = vector_find(&this->ignore_marker, guild_id);
	    if (l != this->ignore_marker.end())
	    {
		this->ignore_marker.erase(l);
	    }
	}

	bool Manager::has_ignore_marker(const dpp::snowflake& guild_id) {
	    std::lock_guard<std::mutex> lk(this->im_m);
	    auto l = vector_find(&this->ignore_marker, guild_id);
	    if (l != this->ignore_marker.end()) return true;
	    else return false;
	}
    }
}
