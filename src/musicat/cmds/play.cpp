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

void
slash_run (const dpp::slashcommand_t &event)
{
    auto player_manager = cmd_pre_get_player_manager_ready (event);
    if (player_manager == NULL)
        return;

    bool debug = get_debug_state ();

    auto guild_id = event.command.guild_id;
    auto from = event.from;
    auto user_id = event.command.usr.id;
    dpp::snowflake sha_id = get_sha_id ();

    std::string arg_query = "";
    int64_t arg_top = 0;
    int64_t arg_slip = 0;

    get_inter_param (event, "query", &arg_query);
    get_inter_param (event, "top", &arg_top);
    get_inter_param (event, "slip", &arg_slip);

    dpp::guild *g = dpp::find_guild (guild_id);
    std::pair<dpp::channel *, std::map<dpp::snowflake, dpp::voicestate> >
        vcuser;

    vcuser = get_voice (g, user_id);
    if (!vcuser.first)
        return event.reply ("Join a voice channel first you dummy");

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
            event.reply ("I have no permission to join your voice channel");
            return;
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
                return event.reply (
                    "Sorry but I'm already in another voice channel");

            vcclient_cont = false;

            if (v)
                {
                    std::cerr
                        << "Disconnecting as no member in vc: " << guild_id
                        << " connecting "
                           "to "
                        << vcuser.first->id << '\n';

                    if (v && v->voiceclient
                        && v->voiceclient->get_secs_remaining () > 0.05f)
                        {
                            if (!v->voiceclient->terminating)
                                {
                                    v->voiceclient->pause_audio (false);
                                    v->voiceclient->skip_to_next_marker ();
                                }

                            player_manager->stop_stream (guild_id);
                        }

                    // reconnect
                    player_manager->full_reconnect (
                        from, guild_id, vcclient.first->id, vcuser.first->id);

                    reconnecting = true;
                }
        }

    bool resumed = false;
    const bool no_query = arg_query.empty ();

    if (v && v->voiceclient && v->voiceclient->is_paused ()
        && v->channel_id == vcuser.first->id)
        {
            player_manager->unpause (v->voiceclient, event.command.guild_id);
            if (no_query)
                {
                    event.reply ("Resumed");
                    resumed = true;
                }
        }

    bool continued = false;

    auto guild_player = player_manager->get_player (event.command.guild_id);
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
        return;

    else if (no_query)
        {
            if (!continued)
                {
                    event.reply ("Provide song query if you wanna add a song, "
                                 "may be URL or song name");
                    return;
                }

            // continued true from here

            if (!guild_player->tried_continuing || reconnecting)
                {
                    guild_player->tried_continuing = true;

                    event.reply ("Playback continued");
                    return;
                }

            event.reply ("Seems like I'm broken, lemme fix myself brb");

            // reconnect
            player_manager->full_reconnect (from, guild_id, vcclient.first->id,
                                            vcuser.first->id);
            return;
        }

    event.thinking ();

    player::add_track (false, guild_id, arg_query, arg_top, vcclient_cont, v,
                       vcuser.first->id, sha_id, true, from, event, continued,
                       arg_slip);
}

} // musicat::command::play
