#include "musicat/autocomplete.h"
#include "musicat/cmds.h"
#include "musicat/musicat.h"
#include "musicat/yt-playlist.h"
#include "musicat/yt-search.h"
#include <regex>
#include <vector>

namespace musicat
{
namespace command
{
namespace play
{
namespace autocomplete
{
void
query (const dpp::autocomplete_t &event, std::string param,
       player::player_manager_ptr player_manager)
{

    std::vector<std::pair<std::string, std::string> > avail = {};

    const bool no_len = !param.length ();

    std::vector<std::string> get
        = player_manager->get_available_tracks (no_len ? 25U : 0U);
    avail.reserve (get.size ());

    for (const std::string &i : get)
        avail.push_back (std::make_pair (i, i));

    if (!no_len)
        avail = musicat::autocomplete::filter_candidates (avail, param);

    if (get_debug_state ())
        {
            printf ("[play::autocomplete::query] results:\n");
            for (size_t i = 0; i < avail.size (); i++)
                {
                    printf ("%ld: %s\n", i, avail.at (i).first.c_str ());
                }
        }

    musicat::autocomplete::create_response (avail, event);
}
}

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
                .set_auto_complete (true))
        .add_option (
            dpp::command_option (dpp::co_integer, "top",
                                 "Add [this song] to the top [of the queue]")
                .add_choice (dpp::command_option_choice ("Yes", 1))
                .add_choice (dpp::command_option_choice ("No", 0)));
}

void
slash_run (const dpp::interaction_create_t &event,
           player::player_manager_ptr player_manager)
{
    if (!player_manager->voice_ready (event.command.guild_id, event.from,
                                      event.command.usr.id))
        {
            event.reply ("Please wait while I'm getting ready to stream");
            return;
        }

    auto guild_id = event.command.guild_id;
    auto from = event.from;
    auto user_id = event.command.usr.id;
    const dpp::snowflake sha_id = player_manager->sha_id;
    std::string arg_query = "";
    int64_t arg_top = 0;
    get_inter_param (event, "query", &arg_query);
    get_inter_param (event, "top", &arg_top);

    dpp::guild *g = dpp::find_guild (guild_id);
    std::pair<dpp::channel *, std::map<dpp::snowflake, dpp::voicestate> >
        vcuser;
    try
        {
            vcuser = get_voice (g, user_id);
        }
    catch (const char *e)
        {
            event.reply ("Join a voice channel first you dummy");
            return;
        }

    dpp::user *sha_user = dpp::find_user (sha_id);

    uint64_t cperm = g->permission_overwrites (g->base_permissions (sha_user),
                                               sha_user, vcuser.first);
    printf ("c: %ld\npv: %ld\npp: %ld\n", cperm, cperm & dpp::p_view_channel,
            cperm & dpp::p_connect);

    if (!(cperm & dpp::p_view_channel && cperm & dpp::p_connect))
        {
            event.reply ("I have no permission to join your voice channel");
            return;
        }

    std::pair<dpp::channel *, std::map<dpp::snowflake, dpp::voicestate> >
        vcclient;
    // Whether client in vc and vcclient exist
    bool vcclient_cont = true;
    try
        {
            vcclient = get_voice (g, sha_id);
        }
    catch (const char *e)
        {
            vcclient_cont = false;
            if (from->connecting_voice_channels.find (guild_id)
                != from->connecting_voice_channels.end ())
                {
                    printf ("Disconnecting as not in vc but connected state "
                            "still in cache: %ld\n",
                            guild_id);
                    from->disconnect_voice (guild_id);
                }
        }

    // Client voice conn
    dpp::voiceconn *v = from->get_voice (guild_id);
    if (vcclient_cont && v && v->channel_id != vcclient.first->id)
        {
            vcclient_cont = false;
            printf ("Disconnecting as it seems I just got moved to "
                    "different vc and connection not updated yet: %ld\n",
                    guild_id);

            std::lock_guard<std::mutex> lk (player_manager->dc_m);
            player_manager->disconnecting[guild_id] = vcclient.first->id;
            from->disconnect_voice (guild_id);
        }

    if (vcclient_cont && vcclient.first->id != vcuser.first->id)
        {
            if (has_listener (&vcclient.second))
                return event.reply (
                    "Sorry but I'm already in another voice channel");

            vcclient_cont = false;
            if (v)
                {
                    printf ("Disconnecting as no member in vc: %ld connecting "
                            "to %ld\n",
                            guild_id, vcuser.first->id);
                    if (v && v->voiceclient
                        && v->voiceclient->get_secs_remaining () > 0.1)
                        {
                            if (!v->voiceclient->terminating)
                                {
                                    v->voiceclient->pause_audio (false);
                                    v->voiceclient->skip_to_next_marker ();
                                }
                            player_manager->stop_stream (guild_id);
                        }

                    {
                        std::lock_guard<std::mutex> lk (player_manager->dc_m);
                        player_manager->disconnecting[guild_id]
                            = vcclient.first->id;
                        std::lock_guard<std::mutex> lk2 (player_manager->c_m);
                        std::lock_guard<std::mutex> lk3 (player_manager->wd_m);
                        player_manager->connecting.insert_or_assign (
                            guild_id, vcuser.first->id);
                        player_manager->waiting_vc_ready[guild_id] = "0";
                        from->disconnect_voice (guild_id);
                    }

                    std::thread pjt ([player_manager, from, guild_id] () {
                        player_manager->reconnect (from, guild_id);
                    });
                    pjt.detach ();
                }
        }

    bool resumed = false;
    const bool no_query = arg_query.length () == 0;

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

    auto op = player_manager->get_player (event.command.guild_id);
    bool continued = false;
    if (op)
        {
            std::lock_guard<std::mutex> lk (op->q_m);
            if (v && v->voiceclient && op->queue.size ()
                && !v->voiceclient->is_paused ()
                && !v->voiceclient->is_playing ())
                {
                    v->voiceclient->insert_marker ("c");
                    continued = true;
                }
        }

    if (resumed)
        return;
    else if (no_query)
        {
            if (continued)
                event.reply ("Playback continued");
            else
                event.reply ("Provide song query if you wanna add a song, may "
                             "be URL or song name");
            return;
        }
    else
        {
            event.thinking ();

            add_track (false, guild_id, arg_query, arg_top, vcclient_cont, v,
                       vcuser.first->id, sha_id, player_manager, true, from,
                       event, continued);
        }
}

void
add_track (bool playlist, dpp::snowflake guild_id, std::string arg_query,
           int64_t arg_top, bool vcclient_cont, dpp::voiceconn *v,
           const dpp::snowflake channel_id, const dpp::snowflake sha_id,
           player::player_manager_ptr player_manager, bool from_interaction,
           dpp::discord_client *from, const dpp::interaction_create_t event,
           bool continued)
{

    std::vector<yt_search::YTrack> searches
        = playlist ? yt_search::get_playlist (arg_query).entries ()
                   : yt_search::search (arg_query).trackResults ();

    if (searches.begin () == searches.end ())
        {
            if (from_interaction)
                event.edit_response ("Can't find anything");
            return;
        }

    yt_search::YTrack result = {};
    if (playlist == false)
        result = searches.front ();
    else
        {
            auto p = player_manager->get_player (guild_id);
            if (!p)
                return;
            std::lock_guard<std::mutex> lk (p->q_m);
            if (p->queue.begin () == p->queue.end ())
                return;
            for (auto i : searches)
                {
                    auto iid = i.id ();
                    bool br = false;
                    for (auto &a : p->queue)
                        {
                            if (a.id () == iid)
                                {
                                    br = true;
                                    break;
                                }
                        }
                    if (!br)
                        {
                            std::lock_guard<std::mutex> lk (p->h_m);
                            if (p->history.size ())
                                for (const auto &a : p->history)
                                    {
                                        if (a == iid)
                                            {
                                                br = true;
                                                break;
                                            }
                                    }
                        }
                    if (br)
                        continue;
                    result = i;
                    break;
                }
            if (result.raw.is_null ())
                return;
        }

    const std::string fname = std::regex_replace (
        result.title () + std::string ("-") + result.id ()
            + std::string (".opus"),
        std::regex ("/"), "", std::regex_constants::match_any);

    if (from_interaction && (vcclient_cont == false || !v))
        {
            std::lock_guard<std::mutex> lk2 (player_manager->c_m);
            std::lock_guard<std::mutex> lk3 (player_manager->wd_m);
            player_manager->connecting[guild_id] = channel_id;
            printf ("INSERTING WAIT FOR VC READY\n");
            player_manager->waiting_vc_ready[guild_id] = fname;
        }

    bool dling = false;

    std::ifstream test (std::string ("music/") + fname,
                        std::ios_base::in | std::ios_base::binary);
    if (!test.is_open ())
        {
            dling = true;
            if (from_interaction)
                event.edit_response (std::string ("Downloading ")
                                     + result.title ()
                                     + "... Gimme 10 sec ight");

            if (player_manager->waiting_file_download.find (fname)
                == player_manager->waiting_file_download.end ())
                {
                    auto url = result.url ();
                    player_manager->download (fname, url, guild_id);
                }
        }
    else
        {
            test.close ();
            if (from_interaction)
                event.edit_response (std::string ("Added: ")
                                     + result.title ());
        }

    std::thread pjt ([player_manager, from, guild_id] () {
        player_manager->reconnect (from, guild_id);
    });
    pjt.detach ();

    std::thread dlt (
        [player_manager, sha_id, dling, fname, arg_top, from_interaction,
         guild_id, from, continued] (const dpp::interaction_create_t event,
                                     yt_search::YTrack result) {
            dpp::snowflake user_id
                = from_interaction ? event.command.usr.id : sha_id;
            auto p = player_manager->create_player (guild_id);

            const dpp::snowflake channel_id
                = from_interaction ? event.command.channel_id : p->channel_id;

            if (dling)
                {
                    player_manager->wait_for_download (fname);
                    if (from_interaction)
                        event.edit_response (std::string ("Added: ")
                                             + result.title ());
                }
            if (from)
                p->from = from;

            player::MCTrack t (result);
            t.filename = fname;
            t.user_id = user_id;
            p->add_track (t, arg_top ? true : false, guild_id,
                          from_interaction || dling);
            if (from_interaction)
                p->set_channel (channel_id);

            decide_play (from, guild_id, continued);
        },
        event, result);
    dlt.detach ();
}

void
decide_play (dpp::discord_client *from, const dpp::snowflake &guild_id,
             const bool &continued)
{
    if (!from || !from->creator)
        return;
    const dpp::snowflake sha_id = from->creator->me.id;

    std::pair<dpp::channel *, std::map<dpp::snowflake, dpp::voicestate> > vu;
    bool b = true;

    try
        {
            vu = get_voice_from_gid (guild_id, sha_id);
        }
    catch (const char *e)
        {
            b = false;
        }

    if (b && !continued)
        {
            dpp::voiceconn *v = from->get_voice (guild_id);

            if (has_listener (&vu.second) && v && v->voiceclient
                && v->voiceclient->is_ready ())

                if ((!v->voiceclient->is_paused ()
                     && !v->voiceclient->is_playing ())
                    || v->voiceclient->get_secs_remaining () < 0.1)

                    v->voiceclient->insert_marker ("s");
        }
}
}
}
}
