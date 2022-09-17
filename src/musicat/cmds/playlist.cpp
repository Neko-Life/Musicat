#include <libpq-fe.h>
#include "nlohmann/json.hpp"
#include "musicat/cmds.h"
#include "musicat/autocomplete.h"
#include "musicat/db.h"
#include "musicat/pagination.h"

namespace musicat {
    namespace command {
	std::string _get_id_arg(const dpp::interaction_create_t& event) {
	    dpp::command_interaction cmd = event.command.get_command_interaction();

	    dpp::command_value val = cmd.options.at(0).options.at(0).value;
	    return val.index() ? std::get<std::string>(val) : "";
	}

	namespace playlist {
	    namespace autocomplete {
		void id(const dpp::autocomplete_t& event, std::string param) {
		    std::pair<PGresult*, ExecStatusType>
			res = database::get_all_user_playlist(event.command.usr.id, database::gup_name_only);

		    std::vector<std::pair<std::string, std::string>> response = {};

		    if (res.second == PGRES_TUPLES_OK)
		    {
			int rn = 0;
			while (true)
			{
			    if (PQgetisnull(res.first, rn, 0)) break;

			    std::string val = std::string(PQgetvalue(res.first, rn, 0));

			    response.push_back(std::make_pair(val, val));
			    rn++;
			}
		    }

		    database::finish_res(res.first);
		    res.first = nullptr;

		    musicat::autocomplete::create_response(
			    musicat::autocomplete::filter_candidates(response, param), event);
		}
	    }

	    namespace save {
		dpp::command_option get_option_obj() {
		    return dpp::command_option(
			    dpp::co_sub_command,
			    "save",
			    "Save [current playlist]"
			    ).add_option(
				dpp::command_option(
				    dpp::co_string,
				    "id",
				    "Playlist name, must match validation regex /^[0-9a-zA-Z_-]{1,100}$/",
				    true
				    ).set_auto_complete(true)
				);
		}

		void slash_run(const dpp::interaction_create_t& event, player::player_manager_ptr player_manager) {
		    std::deque<player::MCTrack> q = player_manager->get_queue(event.command.guild_id);
		    size_t q_size = q.size();
		    if (!q_size)
		    {
			event.reply("Not a single track currently waiting in the queue");
			return;
		    }

		    const std::string p_id = _get_id_arg(event);

		    if (!database::valid_name(p_id))
		    {
			event.reply("Invalid `id` format!");
			return;
		    }

		    if (database::create_table_playlist(event.command.usr.id) == PGRES_FATAL_ERROR)
		    {
			event.reply("`[FATAL]` INTERNAL MUSICAT ERROR!");
			return;
		    }

		    event.thinking();

		    ExecStatusType res = database::update_user_playlist(event.command.usr.id, p_id, q);

		    event.edit_response(res == PGRES_COMMAND_OK
			    ? std::string("Saved playlist containing ")
			    + std::to_string(q_size)
			    + " track"
			    + (q_size > 1 ? "s" : "")
			    + " with Id: "
			    + p_id
			    : "Somethin went wrong, can't save playlist");
		}
	    }

	    namespace load {
		dpp::command_option get_option_obj() {
		    return dpp::command_option(
			    dpp::co_sub_command,
			    "load",
			    "Load [saved playlist]"
			    ).add_option(
				dpp::command_option(
				    dpp::co_string,
				    "id",
				    "Playlist name [to load]",
				    true
				    ).set_auto_complete(true)
				);
		}

		void slash_run(const dpp::interaction_create_t& event, player::player_manager_ptr player_manager, const bool view) {
		    const std::string p_id = _get_id_arg(event);

		    std::pair<PGresult*, ExecStatusType>
			res = database::get_user_playlist(event.command.usr.id, p_id, database::gup_raw_only);

		    std::pair<std::deque<player::MCTrack>, int>
			playlist_res = database::get_playlist_from_PGresult(res.first);

		    int retnow = 0;

		    if (playlist_res.second == -2)
		    {
			if (!res.first) {
			    if (res.second == (ExecStatusType)-3)
				event.reply("Invalid `id` format!");
			    else {
				event.reply(std::string("`[ERROR]` Unexpected error getting user playlist with code: ") + std::to_string(res.second));
				fprintf(stderr, "[CMD_PLAYLIST_ERROR] Unexpected error database::get_user_playlist with code: %d\n", res.second);
			    }

			} else event.reply("Unknown playlist");

			retnow = 1;
		    }
		    else if (playlist_res.second == -1)
		    {
			event.reply("This playlist is empty, save a new one with the same Id to overwrite it");
			retnow = 1;
		    }

		    database::finish_res(res.first);
		    res.first = nullptr;

		    if (retnow) return;

		    std::shared_ptr<player::Player> p;
		    size_t count = 0;

		    std::deque<player::MCTrack> q = {};

		    if (view != true) p = player_manager->create_player(event.command.guild_id);

		    for (auto& t : playlist_res.first) {
			t.user_id = event.command.usr.id;
			if (view) q.push_back(t);
			else
			{
			    p->add_track(t, false, event.command.guild_id, false);
			    count++;
			}
		    }

		    if (view) paginate::reply_paginated_playlist(event, q, p_id);
		    else
		    {
			if (count) try
			{
			    player_manager->update_info_embed(event.command.guild_id);
			}
			catch (...) {}

			event.reply(std::string("Added ") + std::to_string(count) + " track" + (count > 1 ? "s" : "") + " from playlist " + p_id);

			std::pair<dpp::channel*, std::map<dpp::snowflake, dpp::voicestate>>
			    c;
			bool has_c = false;
			bool no_vc = false;
			try
			{
			    c = get_voice_from_gid(event.command.guild_id, event.command.usr.id);
			    has_c = true;
			}
			catch (...) {}

			try
			{
			    get_voice_from_gid(event.command.guild_id, event.from->creator->me.id);
			}
			catch (...)
			{
			    no_vc = true;
			}

			if (has_c && no_vc && c.first && has_permissions_from_ids(event.command.guild_id,
				    event.from->creator->me.id,
				    c.first->id, { dpp::p_view_channel,dpp::p_connect }))
			{
			    p->set_channel(event.command.channel_id);

			    {
				std::lock_guard<std::mutex> lk(player_manager->c_m);
				std::lock_guard<std::mutex> lk2(player_manager->wd_m);
				player_manager->connecting.insert_or_assign(event.command.guild_id, c.first->id);
				player_manager->waiting_vc_ready.insert_or_assign(event.command.guild_id, "2");
			    }

			    std::thread t([player_manager, event]() {
				    player_manager->reconnect(event.from, event.command.guild_id);
				    });
			    t.detach();
			}
		    }
		}
	    }

	    namespace view {
		dpp::command_option get_option_obj() {
		    return dpp::command_option(
			    dpp::co_sub_command,
			    "view",
			    "View [saved playlist] tracks"
			    ).add_option(
				dpp::command_option(
				    dpp::co_string,
				    "id",
				    "Playlist name [to view]",
				    true
				    ).set_auto_complete(true)
				);
		}

		void slash_run(const dpp::interaction_create_t& event) {
		    load::slash_run(event, {}, true);
		}
	    }

	    namespace delete_ {
		dpp::command_option get_option_obj() {
		    return dpp::command_option(
			    dpp::co_sub_command,
			    "delete",
			    "Delete [saved playlist]"
			    ).add_option(
				dpp::command_option(
				    dpp::co_string,
				    "id",
				    "Playlist name [to delete]",
				    true
				    ).set_auto_complete(true)
				);
		}

		void slash_run(const dpp::interaction_create_t& event) {
		    const std::string p_id = _get_id_arg(event);

		    if (!database::valid_name(p_id))
		    {
			event.reply("Invalid `id` format!");
			return;
		    }

		    if (database::delete_user_playlist(event.command.usr.id, p_id) == PGRES_TUPLES_OK)
			event.reply(std::string("Deleted playlist ") + p_id);
		    else event.reply("Unknown playlist");
		}
	    }

	    dpp::slashcommand get_register_obj(const dpp::snowflake& sha_id) {
		return dpp::slashcommand("playlist", "Your playlist manager", sha_id)
		    .add_option(
			    save::get_option_obj()
			    ).add_option(
				load::get_option_obj()
				).add_option(
				    view::get_option_obj()
				    ).add_option(
					delete_::get_option_obj()
					);
	    }

	    void slash_run(const dpp::interaction_create_t& event, player::player_manager_ptr player_manager) {
		auto inter = event.command.get_command_interaction();

		if (inter.options.begin() == inter.options.end())
		{
		    fprintf(stderr, "[ERROR] !!! No options for command 'playlist' !!!\n");
		    event.reply("I don't have that command, how'd you get that?? Please report this to my developer");
		    return;
		}

		const std::string cmd = inter.options.at(0).name;
		if (cmd == "save") save::slash_run(event, player_manager);
		else if (cmd == "load") load::slash_run(event, player_manager);
		else if (cmd == "view") view::slash_run(event);
		else if (cmd == "delete") delete_::slash_run(event);
		else
		{
		    fprintf(stderr, "[ERROR] !!! NO SUB-COMMAND '%s' FOR COMMAND 'playlist' !!!\n", cmd.c_str());
		    event.reply("Somethin went horribly wrong.... Please quickly report this to my developer");
		}
	    }
	}
    }
}
