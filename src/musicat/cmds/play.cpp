#include "musicat/cmds/play.h"
#include "musicat/autocomplete.h"
#include "musicat/cmds.h"
#include "musicat/musicat.h"
#include "musicat/search-cache.h"
#include "musicat/util.h"
#include <memory>
#include <vector>

namespace musicat::command::play
{
namespace autocomplete
{
void
query (const dpp::autocomplete_t &event, const std::string &param)
{
    std::vector<std::pair<std::string, std::string> > avail = {};

    bool no_len = param.empty ();

    const std::vector<player::gat_t> get
        = player::get_available_tracks (no_len ? 25U : 0U);

    avail.reserve (get.size ());

    for (const player::gat_t &i : get)
        avail.emplace_back (i.name, i.name);

    if (!no_len)
        avail = musicat::autocomplete::filter_candidates (avail, param);

    if (get_debug_state ())
        {
            util::print_autocomplete_results (avail,
                                              "play::autocomplete::query");
        }

    musicat::autocomplete::create_response (avail, event);
}
} // autocomplete

dpp::slashcommand
get_register_obj (const dpp::snowflake &sha_id)
{
    return dpp::slashcommand ("play",
                              "Play [a song], resume [paused playback], or "
                              "add [song to queue]",
                              sha_id)
        .add_option (
            dpp::command_option (dpp::co_string, "query",
                                 "Song [to search] or Youtube URL [to play]")
                .set_auto_complete (true)
                .set_max_length (150))
        .add_option (create_yes_no_option (
            "top", "Add [this song] to the top [of the queue]"))
        .add_option (dpp::command_option (dpp::co_integer, "slip",
                                          "Slip [this song to this] position "
                                          "[in the queue]"));
}

int
run (dpp::discord_client *from, const dpp::snowflake &user_id,
     const dpp::snowflake &guild_id, const dpp::interaction_create_t &event,
     std::string &out, const std::string &arg_query = "", int64_t arg_top = 0,
     int64_t arg_slip = 0, bool button_run = false,
     bool update_info_embed = true)
{
    auto pm_res = cmd_pre_get_player_manager_ready_werr (guild_id);
    if (pm_res.second == 1)
        {
            out = "Please wait while I'm getting ready to stream";
            return 1;
        }

    auto player_manager = pm_res.first;

    if (player_manager == NULL)
        return -1;

    bool debug = get_debug_state ();

    const dpp::snowflake sha_id = get_sha_id ();

    dpp::guild *g = dpp::find_guild (guild_id);
    std::pair<dpp::channel *, std::map<dpp::snowflake, dpp::voicestate> >
        vcuser;

    vcuser = get_voice (g, user_id);
    if (!vcuser.first)
        {
            out = "Join a voice channel first you dummy";
            return 1;
        }

    dpp::user const *sha_user = dpp::find_user (sha_id);

    uint64_t cperm = g->permission_overwrites (g->base_permissions (sha_user),
                                               sha_user, vcuser.first);

    if (debug)
        {
            fprintf (stderr, "c: %ld\npv: %ld\npp: %ld\n", cperm,
                     cperm & dpp::p_view_channel, cperm & dpp::p_connect);
        }

    if (!(cperm & dpp::p_view_channel && cperm & dpp::p_connect))
        {
            out = "I have no permission to join your voice channel";
            return 1;
        }

    std::pair<dpp::channel *, std::map<dpp::snowflake, dpp::voicestate> >
        vcclient;

    vcclient = get_voice (g, sha_id);
    // Whether client in vc and vcclient exist
    bool vcclient_cont = vcclient.first != nullptr;
    if (!vcclient_cont
        && from->connecting_voice_channels.find (guild_id)
               != from->connecting_voice_channels.end ())
        {
            std::cerr << "Disconnecting as not in vc but connected state "
                         "still in cache: "
                      << guild_id << '\n';

            from->disconnect_voice (guild_id);
        }

    // Client voice conn
    dpp::voiceconn *v = from->get_voice (guild_id);
    if (vcclient_cont && v && v->channel_id != vcclient.first->id)
        {
            vcclient_cont = false;
            std::cerr << "Disconnecting as it seems I just got moved to "
                         "different vc and connection not updated yet: "
                      << guild_id << "\n";

            player_manager->set_disconnecting (guild_id, vcclient.first->id);

            from->disconnect_voice (guild_id);
        }

    bool reconnecting = false;

    if (vcclient_cont && vcclient.first->id != vcuser.first->id)
        {
            if (has_listener (&vcclient.second))
                {
                    out = "Sorry but I'm already in another voice channel";
                    return 1;
                }
            vcclient_cont = false;

            player_manager->full_reconnect (from, guild_id, vcclient.first->id,
                                            vcuser.first->id);
            reconnecting = true;
        }

    bool resumed = false;
    const bool no_query = arg_query.empty ();

    if (v && v->voiceclient && v->voiceclient->is_paused ()
        && v->channel_id == vcuser.first->id)
        {
            player_manager->unpause (v->voiceclient, guild_id,
                                     update_info_embed);
            if (no_query)
                {
                    out = "Resumed";
                    resumed = true;
                }
        }

    bool continued = false;

    auto guild_player = player_manager->get_player (guild_id);
    if (guild_player)
        {
            std::lock_guard lk (guild_player->t_mutex);
            if (v && v->voiceclient && guild_player->queue.size ()
                && !v->voiceclient->is_paused ()
                && v->voiceclient->get_secs_remaining () < 0.05f)
                {
                    v->voiceclient->stop_audio ();
                    v->voiceclient->insert_marker ("c");
                    continued = true;
                }

            if (resumed)
                guild_player->tried_continuing = true;
        }

    if (resumed)
        return 1;

    else if (no_query)
        {
            if (!continued)
                {
                    out = "Provide song query if you wanna add a song, "
                          "may be URL or song name";
                    return 1;
                }

            // continued true from here

            if (!guild_player->tried_continuing || reconnecting)
                {
                    guild_player->tried_continuing = true;

                    out = "`[SUS]` Something's fishy going on with my mixer";
                    return 1;
                }

            out = "`[CRAP]` Seems like I'm broken, lemme fix myself brb";

            // reconnect
            player_manager->full_reconnect (from, guild_id, vcclient.first->id,
                                            vcuser.first->id);
            return 1;
        }

    if (button_run)
        return -2;

    event.thinking ();
    player::add_track (false, guild_id, arg_query, arg_top, vcclient_cont, v,
                       vcuser.first->id, sha_id, true, from, event, continued,
                       arg_slip);

    return 0;
}

void
slash_run (const dpp::slashcommand_t &event)
{
    auto from = event.from;
    auto user_id = event.command.usr.id;
    auto guild_id = event.command.guild_id;

    std::string arg_query = "";
    int64_t arg_top = 0;
    int64_t arg_slip = 0;

    get_inter_param (event, "query", &arg_query);
    get_inter_param (event, "top", &arg_top);
    get_inter_param (event, "slip", &arg_slip);

    std::string out;
    int status = run (from, user_id, guild_id, event, out,

                      arg_query, arg_top, arg_slip);

    if (status == 1)
        {
            event.reply (out);
        }
}

void
button_run (const dpp::button_click_t &event)
{
    auto player_manager = get_player_manager_ptr ();
    if (player_manager == NULL)
        return;

    auto from = event.from;
    auto user_id = event.command.usr.id;
    auto guild_id = event.command.guild_id;

    std::string arg_query = "";
    int64_t arg_top = 0;
    int64_t arg_slip = 0;

    std::string out;
    run (from, user_id, guild_id, event, out, arg_query, arg_top, arg_slip,
         true, false);

    player_manager->update_info_embed (event.command.guild_id, false, &event);
}

} // musicat::command::play
