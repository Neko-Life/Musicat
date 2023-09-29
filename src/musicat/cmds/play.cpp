// !TODO: REFACTOR THIS HORRIBLE FILE

#include "musicat/autocomplete.h"
#include "musicat/cmds.h"
#include "musicat/musicat.h"
#include "musicat/search-cache.h"
#include "musicat/thread_manager.h"
#include "musicat/util.h"
#include "yt-search/yt-playlist.h"
#include "yt-search/yt-search.h"
#include <memory>
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
                .set_auto_complete (true))
        .add_option (
            dpp::command_option (dpp::co_integer, "top",
                                 "Add [this song] to the top [of the queue]")
                .add_choice (dpp::command_option_choice ("Yes", 1))
                .add_choice (dpp::command_option_choice ("No", 0)))
        .add_option (dpp::command_option (dpp::co_integer, "slip",
                                          "Slip [this song to this] position "
                                          "[in the queue]"));
}

void
slash_run (const dpp::slashcommand_t &event)
{
    auto player_manager = get_player_manager_ptr ();
    if (!player_manager)
        {
            return;
        }

    if (!player_manager->voice_ready (event.command.guild_id, event.from,
                                      event.command.usr.id))
        {
            event.reply ("Please wait while I'm getting ready to stream");
            return;
        }

    const bool debug = get_debug_state ();

    auto guild_id = event.command.guild_id;
    auto from = event.from;
    auto user_id = event.command.usr.id;
    const dpp::snowflake sha_id = get_sha_id ();

    std::string arg_query = "";
    int64_t arg_top = 0;
    int64_t arg_slip = 0;

    get_inter_param (event, "query", &arg_query);
    get_inter_param (event, "top", &arg_top);
    get_inter_param (event, "slip", &arg_slip);

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
    fprintf (stderr, "c: %ld\npv: %ld\npp: %ld\n", cperm,
             cperm & dpp::p_view_channel, cperm & dpp::p_connect);

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
                    fprintf (stderr,
                             "Disconnecting as not in vc but connected state "
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
            fprintf (stderr,
                     "Disconnecting as it seems I just got moved to "
                     "different vc and connection not updated yet: %ld\n",
                     guild_id);

            player_manager->set_disconnecting (guild_id, vcclient.first->id);

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
                    fprintf (
                        stderr,
                        "Disconnecting as no member in vc: %ld connecting "
                        "to %ld\n",
                        guild_id, vcuser.first->id);

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

    bool continued = false;

    auto guild_player = player_manager->get_player (event.command.guild_id);
    if (guild_player)
        {
            if (debug)
                fprintf (stderr,
                         "[play::slash_run] Locked player::t_mutex: %ld\n",
                         guild_player->guild_id);

            std::lock_guard<std::mutex> lk (guild_player->t_mutex);
            if (v && v->voiceclient && guild_player->queue.size ()
                && !v->voiceclient->is_paused ()
                && v->voiceclient->get_secs_remaining () < 0.05f)
                {
                    v->voiceclient->stop_audio ();
                    v->voiceclient->insert_marker ("c");
                    continued = true;
                }

            if (debug)
                fprintf (
                    stderr,
                    "[play::slash_run] Should unlock player::t_mutex: %ld\n",
                    guild_player->guild_id);
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
                       vcuser.first->id, sha_id, true, from, event, continued,
                       arg_slip);
        }
}

std::pair<yt_search::YTrack, int>
find_track (bool playlist, std::string &arg_query,
            player::player_manager_ptr player_manager, bool from_interaction,
            dpp::snowflake guild_id, bool no_check_history,
            const std::string &cache_id)
{
    bool debug = get_debug_state ();
    bool has_cache_id = cache_id.length ();

    std::shared_ptr<player::Player> guild_player = NULL;

    // i wonder what was this for... i do the one who wrote this but i forgot
    if (playlist && !no_check_history)
        {
            guild_player = player_manager->get_player (guild_id);

            if (!guild_player)
                return { {}, 1 };

            // if there's no track return without searching first?
            std::lock_guard<std::mutex> lk (guild_player->t_mutex);
            if (guild_player->queue.begin () == guild_player->queue.end ())
                {
                    return { {}, 1 };
                }
        }

    yt_search::YSearchResult search_result = {};

    // prioritize cache over searching
    std::vector<yt_search::YTrack> searches;

    if (has_cache_id)
        searches = search_cache::get (cache_id);

    size_t searches_size = has_cache_id ? searches.size () : 0;
    // quick decide to remove when no result found instead of looking up in the
    // cache map
    size_t cached_size = has_cache_id ? searches_size : 0;

    bool searched = false;

    // cache not found or no cache Id provided, lets search
    if (!searches_size)
        {
            searches = playlist
                           ? yt_search::get_playlist (arg_query).entries ()
                           : (search_result = yt_search::search (arg_query))
                                 .trackResults ();

            searches_size = searches.size ();

            if (!playlist && !searches_size)
                // desperate to get a track
                // get_playlist already do this if no track from default
                // result found
                searches = search_result.sideTrackPlaylist ();

            searched = true;
        }

    searches_size = searches.size ();

    // save the result to cache
    if (searched && has_cache_id && searches_size)
        search_cache::set (cache_id, searches);

    if (searches.begin () == searches.end ())
        {
            if (from_interaction)
                return { {}, -1 };

            return { {}, 0 };
        }

    yt_search::YTrack result = {};
    if (playlist == false || no_check_history)
        // play the first result according to user query
        result = searches.front ();
    else if (!no_check_history)
        {
            size_t gphs = guild_player->history.size ();

            // find entry that wasn't played before
            for (const auto &i : searches)
                {
                    auto iid = i.id ();
                    bool br = false;

                    // lookup in current queue
                    for (const auto &a : guild_player->queue)
                        {
                            if (a.id () != iid)
                                continue;

                            br = true;
                            break;
                        }

                    if (gphs && !br)
                        {
                            // lookup in history
                            for (const auto &a : guild_player->history)
                                {
                                    if (a != iid)
                                        continue;

                                    br = true;
                                    break;
                                }
                        }

                    // current entry is in the queue or has ever been played in
                    // the last N history
                    // don't pick it
                    if (br)
                        continue;

                    result = i;
                    break;
                }

            if (debug)
                fprintf (
                    stderr,
                    "[play::find_track] Should unlock player::t_mutex: %ld\n",
                    guild_player->guild_id);

            if (result.raw.is_null ())
                {
                    // invalidate cache if Id provided
                    if (has_cache_id && cached_size)
                        search_cache::remove (cache_id);

                    return { {}, 1 };
                }
        }

    return { result, 0 };
}

std::string
get_filename_from_result (yt_search::YTrack &result)
{
    const auto sid = result.id ();
    const auto st = result.title ();

    // ignore title for now, this is definitely problematic
    // if we want to support other track fetching method eg. radio url
    if (!sid.length () /* || !st.length()*/)
        return "";

    return std::regex_replace (
        st + std::string ("-") + sid + std::string (".opus"), std::regex ("/"),
        "", std::regex_constants::match_any);
}

std::pair<bool, int>
track_exist (const std::string &fname, const std::string &url,
             player::player_manager_ptr player_manager, bool from_interaction,
             dpp::snowflake guild_id, bool no_download)
{
    if (!fname.length ())
        return { false, 2 };

    bool dling = false;
    int status = 0;

    std::ifstream test (get_music_folder_path () + fname,
                        std::ios_base::in | std::ios_base::binary);

    if (!test.is_open ())
        {
            dling = true;
            if (from_interaction)
                status = 1;

            if (!no_download
                && player_manager->waiting_file_download.find (fname)
                       == player_manager->waiting_file_download.end ())
                {
                    player_manager->download (fname, url, guild_id);
                }
        }
    else
        {
            test.close ();
            if (from_interaction)
                status = 1;
        }

    return { dling, status };
}

void
add_track (bool playlist, dpp::snowflake guild_id, std::string arg_query,
           int64_t arg_top, bool vcclient_cont, dpp::voiceconn *v,
           const dpp::snowflake channel_id, const dpp::snowflake sha_id,
           bool from_interaction, dpp::discord_client *from,
           const dpp::interaction_create_t event, bool continued,
           int64_t arg_slip, const std::string &cache_id)
{
    auto player_manager = get_player_manager_ptr ();
    if (!player_manager)
        {
            fprintf (stderr,
                     "[command::play::add_track WARN] Can't add track with "
                     "query, no manager: %s\n",
                     arg_query.c_str ());

            return;
        }

    const bool debug = get_debug_state ();

    auto find_result
        = find_track (playlist, arg_query, player_manager, from_interaction,
                      guild_id, false, cache_id);

    switch (find_result.second)
        {
        case -1:
            event.edit_response ("Can't find anything");
            return;
        case 1:
            return;
        case 0:
            break;
        default:
            fprintf (stderr,
                     "[command::play::add_track WARN] Unhandled find_track "
                     "return status: %d\n",
                     find_result.second);
        }

    auto result = find_result.first;

    const std::string fname = get_filename_from_result (result);

    if (from_interaction && (vcclient_cont == false || !v))
        {
            player_manager->set_connecting (guild_id, channel_id);
            player_manager->set_waiting_vc_ready (guild_id, fname);
        }

    const auto result_url = result.url ();

    auto download_result = track_exist (fname, result_url, player_manager,
                                        from_interaction, guild_id);
    bool dling = download_result.first;

    switch (download_result.second)
        {
        case 2:
            if (from_interaction)
                {
                    event.edit_response ("`[ERROR]` Unable to find track");
                }

            if (debug)
                fprintf (stderr,
                         "[play::add_track ERROR] Unable to download track: "
                         "`%s` `%s`\n",
                         fname.c_str (), result_url.c_str ());

            return;
        case 1:
            if (dling)
                {
                    event.edit_response (
                        util::response::reply_downloading_track (
                            result.title ()));
                }
            else
                {
                    if (debug)
                        fprintf (stderr,
                                 "track arg_top arg_slip: '%s' %ld %ld\n",
                                 result.title ().c_str (), arg_top, arg_slip);

                    event.edit_response (util::response::reply_added_track (
                        result.title (), arg_top ? arg_top : arg_slip));
                }
        case 0:
            break;
        default:
            fprintf (stderr,
                     "[command::play::add_track WARN] Unhandled track_exist "
                     "return status: %d\n",
                     download_result.second);
        }

    std::thread pjt ([player_manager, from, guild_id] () {
        try
            {
                player_manager->reconnect (from, guild_id);
            }
        catch (...)
            {
            }

        thread_manager::set_done ();
    });

    thread_manager::dispatch (pjt);

    std::thread dlt (
        [player_manager, sha_id, dling, fname, arg_top, from_interaction,
         guild_id, from, continued, arg_slip,
         event] (yt_search::YTrack result) {
            dpp::snowflake user_id
                = from_interaction ? event.command.usr.id : sha_id;
            auto guild_player = player_manager->create_player (guild_id);

            const dpp::snowflake channel_id = from_interaction
                                                  ? event.command.channel_id
                                                  : guild_player->channel_id;

            if (dling)
                {
                    player_manager->wait_for_download (fname);
                    if (from_interaction)
                        event.edit_response (
                            util::response::reply_added_track (
                                result.title (),
                                arg_top ? arg_top : arg_slip));
                }
            if (from)
                guild_player->from = from;

            player::MCTrack t (result);
            t.filename = fname;
            t.user_id = user_id;

            guild_player->add_track (t, arg_top ? true : false, guild_id,
                                     from_interaction || dling, arg_slip);

            if (from_interaction)
                guild_player->set_channel (channel_id);

            decide_play (from, guild_id, continued);
        },
        result);
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
                    || v->voiceclient->get_secs_remaining () < 0.05f)

                    v->voiceclient->insert_marker ("s");
        }
}
} // play
} // command
} // musicat
